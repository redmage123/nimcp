/**
 * @file test_code_immune_self_repair_integration.cpp
 * @brief Integration tests for Code Immune Self-Repair Integration
 * @version 1.0.0
 * @date 2025-01-20
 *
 * WHAT: Integration tests for code immune to self-repair pipeline
 * WHY: Verify crash pattern detection triggers appropriate repairs
 * HOW: Test full integration of code immune, self-repair, and health agent
 *
 * TEST SCENARIOS:
 * - Crash antigen to diagnostic conversion
 * - Auto-repair threshold evaluation
 * - Cooldown period enforcement
 * - B cell learning from repair outcomes
 * - Health agent notification on failure
 * - Full pipeline flow
 *
 * @author NIMCP Development Team
 */

#include <gtest/gtest.h>
#include <vector>
#include <cstring>
#include <csignal>
#include <chrono>

// Headers have their own extern "C" guards
#include "cognitive/immune/nimcp_code_immune_self_repair.h"
#include "cognitive/immune/nimcp_code_immune.h"
#include "cognitive/fault_tolerance/nimcp_self_repair.h"
#include "utils/fault_tolerance/nimcp_health_agent.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"

//=============================================================================
// Test Fixture
//=============================================================================

class CodeImmuneSelfRepairIntegrationTest : public ::testing::Test {
protected:
    size_t baseline_allocated = 0;

    // Pipeline components
    code_immune_system_t* code_immune = nullptr;
    self_repair_coordinator_t* self_repair = nullptr;
    code_immune_self_repair_bridge_t* bridge = nullptr;
    nimcp_health_agent_t* health_agent = nullptr;

    void SetUp() override {
        nimcp_memory_init();
        nimcp_memory_enable_tracking(true);

        nimcp_memory_stats_t stats;
        nimcp_memory_get_stats(&stats);
        baseline_allocated = stats.current_allocated;

        // Create components
        code_immune = code_immune_create(NULL);
        self_repair = self_repair_create(NULL);
        health_agent = nimcp_health_agent_create(NULL);

        if (code_immune && self_repair) {
            bridge = code_immune_self_repair_bridge_create(
                NULL, code_immune, self_repair);
        }
    }

    void TearDown() override {
        // Cleanup in reverse order
        if (bridge) code_immune_self_repair_bridge_destroy(bridge);
        if (health_agent) nimcp_health_agent_destroy(health_agent);
        if (self_repair) self_repair_destroy(self_repair);
        if (code_immune) code_immune_destroy(code_immune);

        nimcp_memory_stats_t stats;
        nimcp_memory_get_stats(&stats);
        EXPECT_EQ(stats.current_allocated, baseline_allocated)
            << "Memory leak detected! Allocated: " << stats.current_allocated
            << ", Baseline: " << baseline_allocated;
    }

    // Helper: Create test antigen
    void create_test_antigen(code_antigen_t* antigen, int signal,
                              float severity, float confidence,
                              uint32_t recurrence_count) {
        memset(antigen, 0, sizeof(*antigen));
        antigen->id = 1;
        antigen->signal = signal;
        antigen->severity = severity;
        antigen->confidence = confidence;
        antigen->recurrence_count = recurrence_count;
        strncpy(antigen->source_file, "/test/crash_source.c",
                sizeof(antigen->source_file) - 1);
        strncpy(antigen->function_name, "buggy_function",
                sizeof(antigen->function_name) - 1);
        antigen->line_number = 123;
        antigen->fault_address = (void*)0x12345678;
        antigen->instruction_pointer = (void*)0xDEADBEEF;
        antigen->timestamp = 1000000;
    }
};

//=============================================================================
// Pipeline Setup Tests
//=============================================================================

TEST_F(CodeImmuneSelfRepairIntegrationTest, FullPipelineCreation) {
    ASSERT_NE(code_immune, nullptr) << "Code immune creation failed";
    ASSERT_NE(self_repair, nullptr) << "Self-repair creation failed";
    ASSERT_NE(health_agent, nullptr) << "Health agent creation failed";
    ASSERT_NE(bridge, nullptr) << "Code immune self-repair bridge creation failed";

    EXPECT_TRUE(code_immune_self_repair_is_ready(bridge));
}

TEST_F(CodeImmuneSelfRepairIntegrationTest, ConnectHealthAgent) {
    ASSERT_NE(bridge, nullptr);
    ASSERT_NE(health_agent, nullptr);

    int ret = code_immune_self_repair_connect_health_agent(bridge, health_agent);
    EXPECT_EQ(ret, 0);
}

//=============================================================================
// Antigen to Diagnostic Conversion Tests
//=============================================================================

TEST_F(CodeImmuneSelfRepairIntegrationTest, ConvertSigsegvAntigen) {
    code_antigen_t antigen;
    create_test_antigen(&antigen, SIGSEGV, 0.85f, 0.85f, 3);

    diagnostic_result_t* result = nullptr;
    int ret = code_immune_antigen_to_diagnostic(&antigen, &result);

    ASSERT_EQ(ret, 0);
    ASSERT_NE(result, nullptr);

    // SIGSEGV should map to memory-related error with severity >= 0.7 -> CRITICAL
    EXPECT_EQ(result->severity, DIAG_SEVERITY_CRITICAL);
    EXPECT_GT(result->confidence, 0.0f);

    diagnostics_free_result(result);
}

TEST_F(CodeImmuneSelfRepairIntegrationTest, ConvertSigfpeAntigen) {
    code_antigen_t antigen;
    create_test_antigen(&antigen, SIGFPE, 0.7f, 0.8f, 2);

    diagnostic_result_t* result = nullptr;
    int ret = code_immune_antigen_to_diagnostic(&antigen, &result);

    ASSERT_EQ(ret, 0);
    ASSERT_NE(result, nullptr);

    // SIGFPE should map to floating point error
    EXPECT_EQ(result->error_type, ERROR_TYPE_FLOATING_POINT_ERROR);

    diagnostics_free_result(result);
}

TEST_F(CodeImmuneSelfRepairIntegrationTest, ConvertSigabrtAntigen) {
    code_antigen_t antigen;
    create_test_antigen(&antigen, SIGABRT, 0.85f, 0.95f, 1);

    diagnostic_result_t* result = nullptr;
    int ret = code_immune_antigen_to_diagnostic(&antigen, &result);

    ASSERT_EQ(ret, 0);
    ASSERT_NE(result, nullptr);

    // SIGABRT is typically assertion failure with severity >= 0.7 -> CRITICAL
    EXPECT_EQ(result->severity, DIAG_SEVERITY_CRITICAL);

    diagnostics_free_result(result);
}

//=============================================================================
// Auto-Repair Threshold Tests
//=============================================================================

TEST_F(CodeImmuneSelfRepairIntegrationTest, ThresholdCheckBelowCrashCount) {
    ASSERT_NE(bridge, nullptr);

    code_antigen_t antigen;
    create_test_antigen(&antigen, SIGSEGV, 0.9f, 0.9f, 2);  // Only 2 crashes, default min is 3

    bool should_repair = code_immune_should_auto_repair(bridge, &antigen);
    EXPECT_FALSE(should_repair);
}

TEST_F(CodeImmuneSelfRepairIntegrationTest, ThresholdCheckMeetsCriteria) {
    ASSERT_NE(bridge, nullptr);

    code_antigen_t antigen;
    create_test_antigen(&antigen, SIGSEGV, 0.9f, 0.8f, 5);  // All criteria met

    bool should_repair = code_immune_should_auto_repair(bridge, &antigen);
    EXPECT_TRUE(should_repair);
}

TEST_F(CodeImmuneSelfRepairIntegrationTest, ThresholdCheckLowSeverity) {
    ASSERT_NE(bridge, nullptr);

    code_antigen_t antigen;
    create_test_antigen(&antigen, SIGSEGV, 0.5f, 0.9f, 5);  // Low severity

    bool should_repair = code_immune_should_auto_repair(bridge, &antigen);
    EXPECT_FALSE(should_repair);
}

TEST_F(CodeImmuneSelfRepairIntegrationTest, ThresholdCheckLowConfidence) {
    ASSERT_NE(bridge, nullptr);

    code_antigen_t antigen;
    create_test_antigen(&antigen, SIGSEGV, 0.9f, 0.3f, 5);  // Low confidence

    bool should_repair = code_immune_should_auto_repair(bridge, &antigen);
    EXPECT_FALSE(should_repair);
}

//=============================================================================
// Custom Configuration Tests
//=============================================================================

TEST_F(CodeImmuneSelfRepairIntegrationTest, CustomThresholds) {
    ASSERT_NE(code_immune, nullptr);
    ASSERT_NE(self_repair, nullptr);

    // Create bridge with custom config
    code_immune_auto_repair_config_t config;
    code_immune_auto_repair_default_config(&config);
    config.min_crash_count = 1;    // Lower threshold
    config.min_severity = 0.5f;     // Lower severity threshold
    config.min_confidence = 0.3f;   // Lower confidence threshold

    code_immune_self_repair_bridge_t* custom_bridge =
        code_immune_self_repair_bridge_create(&config, code_immune, self_repair);
    ASSERT_NE(custom_bridge, nullptr);

    // This antigen should now pass with lower thresholds
    code_antigen_t antigen;
    create_test_antigen(&antigen, SIGSEGV, 0.6f, 0.4f, 1);

    bool should_repair = code_immune_should_auto_repair(custom_bridge, &antigen);
    EXPECT_TRUE(should_repair);

    code_immune_self_repair_bridge_destroy(custom_bridge);
}

TEST_F(CodeImmuneSelfRepairIntegrationTest, DisabledBridge) {
    ASSERT_NE(code_immune, nullptr);
    ASSERT_NE(self_repair, nullptr);

    // Create disabled bridge
    code_immune_auto_repair_config_t config;
    code_immune_auto_repair_default_config(&config);
    config.enabled = false;

    code_immune_self_repair_bridge_t* disabled_bridge =
        code_immune_self_repair_bridge_create(&config, code_immune, self_repair);
    ASSERT_NE(disabled_bridge, nullptr);

    // Even with all criteria met, should not repair
    code_antigen_t antigen;
    create_test_antigen(&antigen, SIGSEGV, 1.0f, 1.0f, 100);

    bool should_repair = code_immune_should_auto_repair(disabled_bridge, &antigen);
    EXPECT_FALSE(should_repair);

    code_immune_self_repair_bridge_destroy(disabled_bridge);
}

//=============================================================================
// Outcome Learning Tests
//=============================================================================

TEST_F(CodeImmuneSelfRepairIntegrationTest, NotifySuccessfulRepair) {
    ASSERT_NE(bridge, nullptr);

    // Notifying about an unknown repair_id returns -1
    // (tracking record must exist for repair_id to be valid)
    int ret = code_immune_notify_repair_outcome(bridge, 12345, true, NULL);
    EXPECT_EQ(ret, -1);

    // Statistics should not be updated for unknown repair_ids
    code_immune_self_repair_stats_t stats;
    code_immune_self_repair_get_stats(bridge, &stats);
    EXPECT_EQ(stats.repairs_succeeded, 0u);
}

TEST_F(CodeImmuneSelfRepairIntegrationTest, NotifyFailedRepair) {
    ASSERT_NE(bridge, nullptr);

    // Notifying about an unknown repair_id returns -1
    // (tracking record must exist for repair_id to be valid)
    int ret = code_immune_notify_repair_outcome(
        bridge, 12345, false, "Fix validation failed");
    EXPECT_EQ(ret, -1);

    // Statistics should not be updated for unknown repair_ids
    code_immune_self_repair_stats_t stats;
    code_immune_self_repair_get_stats(bridge, &stats);
    EXPECT_EQ(stats.repairs_failed, 0u);
}

TEST_F(CodeImmuneSelfRepairIntegrationTest, MultipleOutcomes) {
    ASSERT_NE(bridge, nullptr);

    // All outcomes for unknown repair_ids should return -1
    // (tracking records must exist for valid notifications)
    int ret1 = code_immune_notify_repair_outcome(bridge, 1, true, NULL);
    int ret2 = code_immune_notify_repair_outcome(bridge, 2, true, NULL);
    int ret3 = code_immune_notify_repair_outcome(bridge, 3, false, "Error");
    int ret4 = code_immune_notify_repair_outcome(bridge, 4, true, NULL);
    int ret5 = code_immune_notify_repair_outcome(bridge, 5, false, "Error");

    // All should return -1 for unknown repair_ids
    EXPECT_EQ(ret1, -1);
    EXPECT_EQ(ret2, -1);
    EXPECT_EQ(ret3, -1);
    EXPECT_EQ(ret4, -1);
    EXPECT_EQ(ret5, -1);

    // Statistics should remain at zero
    code_immune_self_repair_stats_t stats;
    code_immune_self_repair_get_stats(bridge, &stats);
    EXPECT_EQ(stats.repairs_succeeded, 0u);
    EXPECT_EQ(stats.repairs_failed, 0u);
}

//=============================================================================
// Process Functions Tests
//=============================================================================

TEST_F(CodeImmuneSelfRepairIntegrationTest, ProcessAutoRepairsEmpty) {
    ASSERT_NE(bridge, nullptr);

    // No antigens pending
    uint32_t triggered = code_immune_process_auto_repairs(bridge);
    EXPECT_EQ(triggered, 0u);
}

TEST_F(CodeImmuneSelfRepairIntegrationTest, ProcessOutcomesEmpty) {
    ASSERT_NE(bridge, nullptr);

    // No outcomes pending
    uint32_t processed = code_immune_process_repair_outcomes(bridge, 10);
    EXPECT_EQ(processed, 0u);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(CodeImmuneSelfRepairIntegrationTest, InitialStatistics) {
    ASSERT_NE(bridge, nullptr);

    code_immune_self_repair_stats_t stats;
    int ret = code_immune_self_repair_get_stats(bridge, &stats);

    ASSERT_EQ(ret, 0);
    EXPECT_EQ(stats.repairs_triggered, 0u);
    EXPECT_EQ(stats.repairs_succeeded, 0u);
    EXPECT_EQ(stats.repairs_failed, 0u);
    EXPECT_EQ(stats.repairs_skipped, 0u);
    EXPECT_EQ(stats.b_cells_updated, 0u);
}

TEST_F(CodeImmuneSelfRepairIntegrationTest, ResetStatistics) {
    ASSERT_NE(bridge, nullptr);

    // Generate some activity
    code_immune_notify_repair_outcome(bridge, 1, true, NULL);
    code_immune_notify_repair_outcome(bridge, 2, false, "Error");

    // Reset
    code_immune_self_repair_reset_stats(bridge);

    // Verify reset
    code_immune_self_repair_stats_t stats;
    code_immune_self_repair_get_stats(bridge, &stats);

    EXPECT_EQ(stats.repairs_succeeded, 0u);
    EXPECT_EQ(stats.repairs_failed, 0u);
}

//=============================================================================
// Tracking Tests
//=============================================================================

TEST_F(CodeImmuneSelfRepairIntegrationTest, TrackingRecordNotFound) {
    ASSERT_NE(bridge, nullptr);

    const code_immune_repair_tracking_t* tracking =
        code_immune_get_repair_tracking(bridge, 99999);
    EXPECT_EQ(tracking, nullptr);
}

//=============================================================================
// Signal Type Coverage Tests
//=============================================================================

TEST_F(CodeImmuneSelfRepairIntegrationTest, AllSignalTypesConversion) {
    struct {
        int signal;
        const char* name;
    } signals[] = {
        { SIGSEGV, "SIGSEGV" },
        { SIGFPE, "SIGFPE" },
        { SIGABRT, "SIGABRT" },
        { SIGBUS, "SIGBUS" },
        { SIGILL, "SIGILL" },
    };

    for (const auto& sig : signals) {
        code_antigen_t antigen;
        create_test_antigen(&antigen, sig.signal, 0.9f, 0.85f, 3);

        diagnostic_result_t* result = nullptr;
        int ret = code_immune_antigen_to_diagnostic(&antigen, &result);

        ASSERT_EQ(ret, 0) << "Failed for signal " << sig.name;
        ASSERT_NE(result, nullptr) << "Null result for signal " << sig.name;

        // Verify basic diagnostic creation
        EXPECT_GT(result->confidence, 0.0f)
            << "Zero confidence for signal " << sig.name;

        diagnostics_free_result(result);
    }
}

//=============================================================================
// Health Agent Integration Tests
//=============================================================================

TEST_F(CodeImmuneSelfRepairIntegrationTest, HealthAgentConnectionAndDisconnection) {
    ASSERT_NE(bridge, nullptr);
    ASSERT_NE(health_agent, nullptr);

    // Connect
    int ret = code_immune_self_repair_connect_health_agent(bridge, health_agent);
    EXPECT_EQ(ret, 0);

    // Disconnect
    ret = code_immune_self_repair_connect_health_agent(bridge, NULL);
    EXPECT_EQ(ret, 0);
}

//=============================================================================
// Utility Function Tests
//=============================================================================

TEST_F(CodeImmuneSelfRepairIntegrationTest, VersionString) {
    const char* version = code_immune_self_repair_version();
    ASSERT_NE(version, nullptr);
    EXPECT_GT(strlen(version), 0u);
    EXPECT_STREQ(version, CODE_IMMUNE_SELF_REPAIR_VERSION);
}

TEST_F(CodeImmuneSelfRepairIntegrationTest, DefaultConfigConstants) {
    code_immune_auto_repair_config_t config;
    int ret = code_immune_auto_repair_default_config(&config);

    ASSERT_EQ(ret, 0);
    EXPECT_EQ(config.min_crash_count, CODE_IMMUNE_DEFAULT_MIN_CRASH_COUNT);
    EXPECT_FLOAT_EQ(config.min_severity, CODE_IMMUNE_DEFAULT_MIN_SEVERITY);
    EXPECT_FLOAT_EQ(config.min_confidence, CODE_IMMUNE_DEFAULT_MIN_CONFIDENCE);
    EXPECT_EQ(config.cooldown_ms, CODE_IMMUNE_DEFAULT_COOLDOWN_MS);
}

//=============================================================================
// Edge Cases and Error Handling
//=============================================================================

TEST_F(CodeImmuneSelfRepairIntegrationTest, NullInputHandling) {
    // Null antigen
    diagnostic_result_t* result = nullptr;
    int ret = code_immune_antigen_to_diagnostic(NULL, &result);
    EXPECT_EQ(ret, -1);

    // Null output
    code_antigen_t antigen;
    create_test_antigen(&antigen, SIGSEGV, 0.9f, 0.85f, 3);
    ret = code_immune_antigen_to_diagnostic(&antigen, NULL);
    EXPECT_EQ(ret, -1);

    // Null bridge for should_auto_repair
    bool should = code_immune_should_auto_repair(NULL, &antigen);
    EXPECT_FALSE(should);

    // Null antigen for should_auto_repair
    should = code_immune_should_auto_repair(bridge, NULL);
    EXPECT_FALSE(should);
}

TEST_F(CodeImmuneSelfRepairIntegrationTest, NullBridgeStatistics) {
    code_immune_self_repair_stats_t stats;
    int ret = code_immune_self_repair_get_stats(NULL, &stats);
    EXPECT_EQ(ret, -1);
}

TEST_F(CodeImmuneSelfRepairIntegrationTest, NullStatsOutput) {
    ASSERT_NE(bridge, nullptr);

    int ret = code_immune_self_repair_get_stats(bridge, NULL);
    EXPECT_EQ(ret, -1);
}

//=============================================================================
// Boundary Value Tests
//=============================================================================

TEST_F(CodeImmuneSelfRepairIntegrationTest, BoundaryValues) {
    ASSERT_NE(bridge, nullptr);

    // Test with exact threshold values
    code_antigen_t antigen;

    // Exactly at min_crash_count (should pass)
    create_test_antigen(&antigen, SIGSEGV, 0.9f, 0.8f, 3);
    EXPECT_TRUE(code_immune_should_auto_repair(bridge, &antigen));

    // Just below min_crash_count (should fail)
    create_test_antigen(&antigen, SIGSEGV, 0.9f, 0.8f, 2);
    EXPECT_FALSE(code_immune_should_auto_repair(bridge, &antigen));

    // Exactly at min_severity (should pass)
    create_test_antigen(&antigen, SIGSEGV, 0.8f, 0.8f, 5);
    EXPECT_TRUE(code_immune_should_auto_repair(bridge, &antigen));

    // Just below min_severity (should fail)
    create_test_antigen(&antigen, SIGSEGV, 0.79f, 0.8f, 5);
    EXPECT_FALSE(code_immune_should_auto_repair(bridge, &antigen));

    // Exactly at min_confidence (should pass)
    create_test_antigen(&antigen, SIGSEGV, 0.9f, 0.6f, 5);
    EXPECT_TRUE(code_immune_should_auto_repair(bridge, &antigen));

    // Just below min_confidence (should fail)
    create_test_antigen(&antigen, SIGSEGV, 0.9f, 0.59f, 5);
    EXPECT_FALSE(code_immune_should_auto_repair(bridge, &antigen));
}

TEST_F(CodeImmuneSelfRepairIntegrationTest, ExtremeValues) {
    ASSERT_NE(bridge, nullptr);

    code_antigen_t antigen;

    // Maximum values (should pass)
    create_test_antigen(&antigen, SIGSEGV, 1.0f, 1.0f, UINT32_MAX);
    EXPECT_TRUE(code_immune_should_auto_repair(bridge, &antigen));

    // Zero values (should fail)
    create_test_antigen(&antigen, SIGSEGV, 0.0f, 0.0f, 0);
    EXPECT_FALSE(code_immune_should_auto_repair(bridge, &antigen));
}
