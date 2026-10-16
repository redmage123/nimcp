/**
 * @file test_code_immune_self_repair.cpp
 * @brief Unit tests for Code Immune Self-Repair Integration
 * @version 1.0.0
 * @date 2025-01-20
 *
 * WHAT: Unit tests for code immune to self-repair pipeline integration
 * WHY: Ensure reliable auto-repair triggering and outcome learning
 * HOW: Test-driven development with coverage of all public APIs
 *
 * Test Coverage:
 * - Creation and destruction
 * - Configuration
 * - Antigen to diagnostic conversion
 * - Auto-repair threshold checks
 * - Outcome notification and learning
 * - Statistics
 *
 * @author NIMCP Development Team
 */

#include <gtest/gtest.h>
#include <cstring>

// Headers have their own extern "C" guards
#include "cognitive/immune/nimcp_code_immune_self_repair.h"
#include "cognitive/immune/nimcp_code_immune.h"
#include "cognitive/fault_tolerance/nimcp_self_repair.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_macros.h"

//=============================================================================
// Test Fixture
//=============================================================================

class CodeImmuneSelfRepairTest : public ::testing::Test {
protected:
    size_t baseline_allocated = 0;
    code_immune_system_t* code_immune = nullptr;
    self_repair_coordinator_t* self_repair = nullptr;

    void SetUp() override {
        nimcp_memory_init();
        nimcp_memory_enable_tracking(true);

        // Force-initialize exception system singletons so their allocations
        // are part of the baseline (handler mutex, exception mutex, etc.)
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "warmup");
        nimcp_exception_clear_current();

        nimcp_memory_stats_t stats;
        nimcp_memory_get_stats(&stats);
        baseline_allocated = stats.current_allocated;

        // Create dependencies
        code_immune = code_immune_create(NULL);
        self_repair = self_repair_create(NULL);
    }

    void TearDown() override {
        // Cleanup dependencies
        if (self_repair) self_repair_destroy(self_repair);
        if (code_immune) code_immune_destroy(code_immune);

        // Clear thread-local exception state (exceptions from bio_router
        // not being initialized leave a ref that appears as a memory leak)
        nimcp_exception_clear_current();

        nimcp_memory_stats_t stats;
        nimcp_memory_get_stats(&stats);
        EXPECT_LE(stats.current_allocated, baseline_allocated + 256)
            << "Memory leak detected! Allocated: " << stats.current_allocated
            << ", Baseline: " << baseline_allocated;
    }

    // Helper to create a test antigen
    void create_test_antigen(code_antigen_t* antigen, int signal,
                             float severity, float confidence,
                             uint32_t recurrence_count) {
        memset(antigen, 0, sizeof(*antigen));
        antigen->id = 1;
        antigen->signal = signal;
        antigen->severity = severity;
        antigen->confidence = confidence;
        antigen->recurrence_count = recurrence_count;
        strncpy(antigen->source_file, "/test/source.c",
                sizeof(antigen->source_file) - 1);
        strncpy(antigen->function_name, "test_function",
                sizeof(antigen->function_name) - 1);
        antigen->line_number = 42;
    }
};

//=============================================================================
// Creation and Destruction Tests
//=============================================================================

TEST_F(CodeImmuneSelfRepairTest, CreateWithDefaults) {
    ASSERT_NE(code_immune, nullptr);
    ASSERT_NE(self_repair, nullptr);

    code_immune_self_repair_bridge_t* bridge =
        code_immune_self_repair_bridge_create(NULL, code_immune, self_repair);

    ASSERT_NE(bridge, nullptr);
    EXPECT_TRUE(code_immune_self_repair_is_ready(bridge));

    code_immune_self_repair_bridge_destroy(bridge);
}

TEST_F(CodeImmuneSelfRepairTest, CreateWithCustomConfig) {
    ASSERT_NE(code_immune, nullptr);
    ASSERT_NE(self_repair, nullptr);

    code_immune_auto_repair_config_t config;
    code_immune_auto_repair_default_config(&config);

    config.enabled = true;
    config.min_crash_count = 5;
    config.min_severity = 0.9f;
    config.min_confidence = 0.7f;
    config.cooldown_ms = 10000;
    config.learn_from_outcomes = true;

    code_immune_self_repair_bridge_t* bridge =
        code_immune_self_repair_bridge_create(&config, code_immune, self_repair);

    ASSERT_NE(bridge, nullptr);
    EXPECT_TRUE(code_immune_self_repair_is_ready(bridge));

    code_immune_self_repair_bridge_destroy(bridge);
}

TEST_F(CodeImmuneSelfRepairTest, CreateNullCodeImmuneReturnsNull) {
    code_immune_self_repair_bridge_t* bridge =
        code_immune_self_repair_bridge_create(NULL, NULL, self_repair);
    EXPECT_EQ(bridge, nullptr);
}

TEST_F(CodeImmuneSelfRepairTest, CreateNullSelfRepairReturnsNull) {
    code_immune_self_repair_bridge_t* bridge =
        code_immune_self_repair_bridge_create(NULL, code_immune, NULL);
    EXPECT_EQ(bridge, nullptr);
}

TEST_F(CodeImmuneSelfRepairTest, DestroyNullSafety) {
    EXPECT_NO_THROW(code_immune_self_repair_bridge_destroy(NULL));
}

//=============================================================================
// Configuration Tests
//=============================================================================

TEST_F(CodeImmuneSelfRepairTest, DefaultConfigValues) {
    code_immune_auto_repair_config_t config;
    int ret = code_immune_auto_repair_default_config(&config);

    ASSERT_EQ(ret, 0);
    EXPECT_TRUE(config.enabled);
    EXPECT_EQ(config.min_crash_count, CODE_IMMUNE_DEFAULT_MIN_CRASH_COUNT);
    EXPECT_FLOAT_EQ(config.min_severity, CODE_IMMUNE_DEFAULT_MIN_SEVERITY);
    EXPECT_FLOAT_EQ(config.min_confidence, CODE_IMMUNE_DEFAULT_MIN_CONFIDENCE);
    EXPECT_EQ(config.cooldown_ms, CODE_IMMUNE_DEFAULT_COOLDOWN_MS);
    EXPECT_TRUE(config.learn_from_outcomes);
}

TEST_F(CodeImmuneSelfRepairTest, DefaultConfigNullReturnsError) {
    int ret = code_immune_auto_repair_default_config(NULL);
    EXPECT_EQ(ret, -1);
}

//=============================================================================
// Antigen to Diagnostic Conversion Tests
//=============================================================================

TEST_F(CodeImmuneSelfRepairTest, ConvertSigsegvAntigen) {
    code_antigen_t antigen;
    create_test_antigen(&antigen, SIGSEGV, 0.9f, 0.85f, 3);

    diagnostic_result_t* result = NULL;
    int ret = code_immune_antigen_to_diagnostic(&antigen, &result);

    ASSERT_EQ(ret, 0);
    ASSERT_NE(result, nullptr);
    // Severity 0.9 maps to FATAL (>= 0.9), not CRITICAL
    EXPECT_EQ(result->severity, DIAG_SEVERITY_FATAL);
    EXPECT_GT(result->confidence, 0.0f);

    diagnostics_free_result(result);
}

TEST_F(CodeImmuneSelfRepairTest, ConvertSigfpeAntigen) {
    code_antigen_t antigen;
    create_test_antigen(&antigen, SIGFPE, 0.7f, 0.8f, 2);

    diagnostic_result_t* result = NULL;
    int ret = code_immune_antigen_to_diagnostic(&antigen, &result);

    ASSERT_EQ(ret, 0);
    ASSERT_NE(result, nullptr);
    // SIGFPE maps to FLOATING_POINT_ERROR signal type
    EXPECT_EQ(result->error_type, ERROR_TYPE_FLOATING_POINT_ERROR);

    diagnostics_free_result(result);
}

TEST_F(CodeImmuneSelfRepairTest, ConvertAntigenNullInputReturnsError) {
    diagnostic_result_t* result = NULL;
    int ret = code_immune_antigen_to_diagnostic(NULL, &result);
    EXPECT_EQ(ret, -1);
}

TEST_F(CodeImmuneSelfRepairTest, ConvertAntigenNullOutputReturnsError) {
    code_antigen_t antigen;
    create_test_antigen(&antigen, SIGSEGV, 0.9f, 0.85f, 3);

    int ret = code_immune_antigen_to_diagnostic(&antigen, NULL);
    EXPECT_EQ(ret, -1);
}

//=============================================================================
// Auto-Repair Threshold Tests
//=============================================================================

TEST_F(CodeImmuneSelfRepairTest, ShouldNotRepairBelowCrashCount) {
    code_immune_auto_repair_config_t config;
    code_immune_auto_repair_default_config(&config);
    config.min_crash_count = 3;

    code_immune_self_repair_bridge_t* bridge =
        code_immune_self_repair_bridge_create(&config, code_immune, self_repair);
    ASSERT_NE(bridge, nullptr);

    code_antigen_t antigen;
    create_test_antigen(&antigen, SIGSEGV, 0.9f, 0.9f, 2);  // Only 2 crashes

    bool should_repair = code_immune_should_auto_repair(bridge, &antigen);
    EXPECT_FALSE(should_repair);

    code_immune_self_repair_bridge_destroy(bridge);
}

TEST_F(CodeImmuneSelfRepairTest, ShouldNotRepairBelowSeverity) {
    code_immune_auto_repair_config_t config;
    code_immune_auto_repair_default_config(&config);
    config.min_severity = 0.8f;

    code_immune_self_repair_bridge_t* bridge =
        code_immune_self_repair_bridge_create(&config, code_immune, self_repair);
    ASSERT_NE(bridge, nullptr);

    code_antigen_t antigen;
    create_test_antigen(&antigen, SIGSEGV, 0.5f, 0.9f, 5);  // Low severity

    bool should_repair = code_immune_should_auto_repair(bridge, &antigen);
    EXPECT_FALSE(should_repair);

    code_immune_self_repair_bridge_destroy(bridge);
}

TEST_F(CodeImmuneSelfRepairTest, ShouldNotRepairBelowConfidence) {
    code_immune_auto_repair_config_t config;
    code_immune_auto_repair_default_config(&config);
    config.min_confidence = 0.6f;

    code_immune_self_repair_bridge_t* bridge =
        code_immune_self_repair_bridge_create(&config, code_immune, self_repair);
    ASSERT_NE(bridge, nullptr);

    code_antigen_t antigen;
    create_test_antigen(&antigen, SIGSEGV, 0.9f, 0.3f, 5);  // Low confidence

    bool should_repair = code_immune_should_auto_repair(bridge, &antigen);
    EXPECT_FALSE(should_repair);

    code_immune_self_repair_bridge_destroy(bridge);
}

TEST_F(CodeImmuneSelfRepairTest, ShouldRepairWhenAllThresholdsMet) {
    code_immune_auto_repair_config_t config;
    code_immune_auto_repair_default_config(&config);
    config.min_crash_count = 3;
    config.min_severity = 0.8f;
    config.min_confidence = 0.6f;

    code_immune_self_repair_bridge_t* bridge =
        code_immune_self_repair_bridge_create(&config, code_immune, self_repair);
    ASSERT_NE(bridge, nullptr);

    code_antigen_t antigen;
    create_test_antigen(&antigen, SIGSEGV, 0.9f, 0.8f, 5);  // All thresholds met

    bool should_repair = code_immune_should_auto_repair(bridge, &antigen);
    EXPECT_TRUE(should_repair);

    code_immune_self_repair_bridge_destroy(bridge);
}

TEST_F(CodeImmuneSelfRepairTest, DisabledBridgeSkipsRepair) {
    code_immune_auto_repair_config_t config;
    code_immune_auto_repair_default_config(&config);
    config.enabled = false;  // Disabled

    code_immune_self_repair_bridge_t* bridge =
        code_immune_self_repair_bridge_create(&config, code_immune, self_repair);
    ASSERT_NE(bridge, nullptr);

    code_antigen_t antigen;
    create_test_antigen(&antigen, SIGSEGV, 0.9f, 0.9f, 10);  // All criteria met

    bool should_repair = code_immune_should_auto_repair(bridge, &antigen);
    EXPECT_FALSE(should_repair);  // Disabled

    code_immune_self_repair_bridge_destroy(bridge);
}

//=============================================================================
// Outcome Notification Tests
//=============================================================================

TEST_F(CodeImmuneSelfRepairTest, NotifyRepairOutcomeSuccess) {
    code_immune_self_repair_bridge_t* bridge =
        code_immune_self_repair_bridge_create(NULL, code_immune, self_repair);
    ASSERT_NE(bridge, nullptr);

    // Notifying about a repair_id that was never triggered returns -1
    // (no tracking record exists for repair_id 12345)
    int ret = code_immune_notify_repair_outcome(bridge, 12345, true, NULL);
    EXPECT_EQ(ret, -1);

    code_immune_self_repair_bridge_destroy(bridge);
}

TEST_F(CodeImmuneSelfRepairTest, NotifyRepairOutcomeFailure) {
    code_immune_self_repair_bridge_t* bridge =
        code_immune_self_repair_bridge_create(NULL, code_immune, self_repair);
    ASSERT_NE(bridge, nullptr);

    // Notifying about a repair_id that was never triggered returns -1
    // (no tracking record exists for repair_id 12345)
    int ret = code_immune_notify_repair_outcome(
        bridge, 12345, false, "Fix validation failed");
    EXPECT_EQ(ret, -1);

    code_immune_self_repair_bridge_destroy(bridge);
}

TEST_F(CodeImmuneSelfRepairTest, NotifyRepairOutcomeNullBridgeReturnsError) {
    int ret = code_immune_notify_repair_outcome(NULL, 12345, true, NULL);
    EXPECT_EQ(ret, -1);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(CodeImmuneSelfRepairTest, GetStatistics) {
    code_immune_self_repair_bridge_t* bridge =
        code_immune_self_repair_bridge_create(NULL, code_immune, self_repair);
    ASSERT_NE(bridge, nullptr);

    code_immune_self_repair_stats_t stats;
    int ret = code_immune_self_repair_get_stats(bridge, &stats);

    ASSERT_EQ(ret, 0);
    EXPECT_EQ(stats.repairs_triggered, 0u);
    EXPECT_EQ(stats.repairs_succeeded, 0u);
    EXPECT_EQ(stats.repairs_failed, 0u);
    EXPECT_EQ(stats.repairs_skipped, 0u);

    code_immune_self_repair_bridge_destroy(bridge);
}

TEST_F(CodeImmuneSelfRepairTest, ResetStatistics) {
    code_immune_self_repair_bridge_t* bridge =
        code_immune_self_repair_bridge_create(NULL, code_immune, self_repair);
    ASSERT_NE(bridge, nullptr);

    code_immune_self_repair_reset_stats(bridge);

    code_immune_self_repair_stats_t stats;
    code_immune_self_repair_get_stats(bridge, &stats);
    EXPECT_EQ(stats.repairs_triggered, 0u);

    code_immune_self_repair_bridge_destroy(bridge);
}

TEST_F(CodeImmuneSelfRepairTest, GetStatisticsNullBridgeReturnsError) {
    code_immune_self_repair_stats_t stats;
    int ret = code_immune_self_repair_get_stats(NULL, &stats);
    EXPECT_EQ(ret, -1);
}

TEST_F(CodeImmuneSelfRepairTest, GetStatisticsNullStatsReturnsError) {
    code_immune_self_repair_bridge_t* bridge =
        code_immune_self_repair_bridge_create(NULL, code_immune, self_repair);
    ASSERT_NE(bridge, nullptr);

    int ret = code_immune_self_repair_get_stats(bridge, NULL);
    EXPECT_EQ(ret, -1);

    code_immune_self_repair_bridge_destroy(bridge);
}

//=============================================================================
// Process Auto-Repairs Tests
//=============================================================================

TEST_F(CodeImmuneSelfRepairTest, ProcessAutoRepairsInitiallyZero) {
    code_immune_self_repair_bridge_t* bridge =
        code_immune_self_repair_bridge_create(NULL, code_immune, self_repair);
    ASSERT_NE(bridge, nullptr);

    uint32_t triggered = code_immune_process_auto_repairs(bridge);
    EXPECT_EQ(triggered, 0u);  // No antigens to process

    code_immune_self_repair_bridge_destroy(bridge);
}

TEST_F(CodeImmuneSelfRepairTest, ProcessRepairOutcomesInitiallyZero) {
    code_immune_self_repair_bridge_t* bridge =
        code_immune_self_repair_bridge_create(NULL, code_immune, self_repair);
    ASSERT_NE(bridge, nullptr);

    uint32_t processed = code_immune_process_repair_outcomes(bridge, 10);
    EXPECT_EQ(processed, 0u);  // No outcomes to process

    code_immune_self_repair_bridge_destroy(bridge);
}

//=============================================================================
// Utility Function Tests
//=============================================================================

TEST_F(CodeImmuneSelfRepairTest, VersionString) {
    const char* version = code_immune_self_repair_version();
    ASSERT_NE(version, nullptr);
    EXPECT_GT(strlen(version), 0u);
}

TEST_F(CodeImmuneSelfRepairTest, IsReadyNullReturnsFalse) {
    EXPECT_FALSE(code_immune_self_repair_is_ready(NULL));
}

//=============================================================================
// Health Agent Connection Tests
//=============================================================================

TEST_F(CodeImmuneSelfRepairTest, ConnectHealthAgentNull) {
    code_immune_self_repair_bridge_t* bridge =
        code_immune_self_repair_bridge_create(NULL, code_immune, self_repair);
    ASSERT_NE(bridge, nullptr);

    // Disconnect by passing NULL
    int ret = code_immune_self_repair_connect_health_agent(bridge, NULL);
    EXPECT_EQ(ret, 0);

    code_immune_self_repair_bridge_destroy(bridge);
}

TEST_F(CodeImmuneSelfRepairTest, ConnectHealthAgentNullBridgeReturnsError) {
    int ret = code_immune_self_repair_connect_health_agent(NULL, NULL);
    EXPECT_EQ(ret, -1);
}

//=============================================================================
// Tracking Tests
//=============================================================================

TEST_F(CodeImmuneSelfRepairTest, GetRepairTrackingNotFound) {
    code_immune_self_repair_bridge_t* bridge =
        code_immune_self_repair_bridge_create(NULL, code_immune, self_repair);
    ASSERT_NE(bridge, nullptr);

    const code_immune_repair_tracking_t* tracking =
        code_immune_get_repair_tracking(bridge, 99999);
    EXPECT_EQ(tracking, nullptr);  // Not found

    code_immune_self_repair_bridge_destroy(bridge);
}

TEST_F(CodeImmuneSelfRepairTest, GetRepairTrackingNullBridge) {
    const code_immune_repair_tracking_t* tracking =
        code_immune_get_repair_tracking(NULL, 12345);
    EXPECT_EQ(tracking, nullptr);
}
