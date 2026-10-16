/**
 * @file test_code_immune_integrated_self_repair.cpp
 * @brief Unit tests for Code Immune Integrated Self-Repair API
 * @version 1.0.0
 * @date 2026-01-20
 *
 * WHAT: Unit tests for integrated self-repair functions in nimcp_code_immune.h
 * WHY: Ensure reliable auto-repair integration directly in code immune
 * HOW: Test connection, triggering, diagnostics, and outcome handling
 *
 * Test Coverage:
 * - Self-repair connection/disconnection
 * - Antigen to diagnostic conversion (integrated API)
 * - Auto-repair eligibility checks
 * - Repair outcome handling
 * - Statistics tracking
 *
 * @author NIMCP Development Team
 */

#include <gtest/gtest.h>
#include <cstring>
#include <signal.h>

// Headers have their own extern "C" guards
#include "cognitive/immune/nimcp_code_immune.h"
#include "cognitive/fault_tolerance/nimcp_self_repair.h"
#include "utils/fault_tolerance/nimcp_diagnostics.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"

//=============================================================================
// Test Fixture
//=============================================================================

class CodeImmuneIntegratedSelfRepairTest : public ::testing::Test {
protected:
    size_t baseline_allocated = 0;
    code_immune_system_t* immune = nullptr;

    void SetUp() override {
        nimcp_memory_init();
        nimcp_memory_enable_tracking(true);

        nimcp_memory_stats_t stats;
        nimcp_memory_get_stats(&stats);
        baseline_allocated = stats.current_allocated;

        // Create code immune system with auto-repair enabled
        code_immune_config_t config;
        code_immune_default_config(&config);
        immune = code_immune_create_with_config(NULL, &config);
    }

    void TearDown() override {
        if (immune) {
            code_immune_destroy(immune);
            immune = nullptr;
        }

        nimcp_memory_stats_t stats;
        nimcp_memory_get_stats(&stats);
        /* Allow small drift from global subsystem lazy-init (exception handlers,
           module registry, etc.) that allocate on first use across tests */
        EXPECT_LE(stats.current_allocated, baseline_allocated + 256)
            << "Memory leak detected! Allocated: " << stats.current_allocated
            << ", Baseline: " << baseline_allocated;
    }

    // Helper to present a crash and get antigen ID
    uint64_t present_test_crash(int signal, float severity = 0.9f) {
        if (!immune) return 0;
        (void)severity;  // Not used in current API

        void* backtrace[4] = {(void*)0x1000, (void*)0x2000, (void*)0x3000, NULL};
        uint64_t antigen_id = 0;

        int ret = code_immune_present_crash_detailed(
            immune,
            signal,
            (void*)0x12345678,      // fault_addr
            (void*)0x1000,          // ip
            "/test/crash.c",        // source_file
            100,                    // line
            "crash_function",       // function
            backtrace,              // backtrace
            3,                      // backtrace_depth
            &antigen_id             // output
        );

        return (ret == 0) ? antigen_id : 0;
    }
};

//=============================================================================
// Connection Tests
//=============================================================================

TEST_F(CodeImmuneIntegratedSelfRepairTest, CreateWithAutoRepairEnabled) {
    ASSERT_NE(immune, nullptr);

    // Auto-repair config should be initialized
    EXPECT_TRUE(immune->config.auto_repair.enabled);
    EXPECT_EQ(immune->config.auto_repair.min_crash_count, 3u);
    EXPECT_FLOAT_EQ(immune->config.auto_repair.min_severity, 0.8f);
}

TEST_F(CodeImmuneIntegratedSelfRepairTest, ConnectDisconnectSelfRepair) {
    ASSERT_NE(immune, nullptr);

    // Initially not connected
    EXPECT_FALSE(code_immune_is_self_repair_connected(immune));

    // Create self-repair coordinator
    self_repair_coordinator_t* self_repair = self_repair_create(NULL);
    ASSERT_NE(self_repair, nullptr);

    // Connect
    int ret = code_immune_connect_self_repair(immune, self_repair);
    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(code_immune_is_self_repair_connected(immune));

    // Disconnect
    ret = code_immune_disconnect_self_repair(immune);
    EXPECT_EQ(ret, 0);
    EXPECT_FALSE(code_immune_is_self_repair_connected(immune));

    self_repair_destroy(self_repair);
}

TEST_F(CodeImmuneIntegratedSelfRepairTest, ConnectNullSystemReturnsError) {
    self_repair_coordinator_t* self_repair = self_repair_create(NULL);
    ASSERT_NE(self_repair, nullptr);

    int ret = code_immune_connect_self_repair(NULL, self_repair);
    EXPECT_EQ(ret, -1);

    self_repair_destroy(self_repair);
}

TEST_F(CodeImmuneIntegratedSelfRepairTest, ConnectNullCoordinatorReturnsError) {
    ASSERT_NE(immune, nullptr);

    int ret = code_immune_connect_self_repair(immune, NULL);
    EXPECT_EQ(ret, -1);
}

TEST_F(CodeImmuneIntegratedSelfRepairTest, DisconnectNullSafe) {
    int ret = code_immune_disconnect_self_repair(NULL);
    EXPECT_EQ(ret, -1);
}

TEST_F(CodeImmuneIntegratedSelfRepairTest, IsConnectedNullReturnsFalse) {
    EXPECT_FALSE(code_immune_is_self_repair_connected(NULL));
}

//=============================================================================
// Antigen to Diagnostic Conversion Tests
//=============================================================================

TEST_F(CodeImmuneIntegratedSelfRepairTest, GetAntigenDiagnostic) {
    ASSERT_NE(immune, nullptr);

    // Present a crash
    uint64_t antigen_id = present_test_crash(SIGSEGV);
    ASSERT_NE(antigen_id, 0u);

    // Convert to diagnostic
    diagnostic_result_t diag;
    int ret = code_immune_get_antigen_diagnostic(immune, antigen_id, &diag);

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(diag.signal_number, SIGSEGV);
    EXPECT_EQ(diag.error_type, ERROR_TYPE_SEGFAULT);
    EXPECT_GT(diag.confidence, 0.0f);
}

TEST_F(CodeImmuneIntegratedSelfRepairTest, GetAntigenDiagnosticNotFound) {
    ASSERT_NE(immune, nullptr);

    diagnostic_result_t diag;
    int ret = code_immune_get_antigen_diagnostic(immune, 99999, &diag);

    EXPECT_EQ(ret, -1);  // Not found
}

TEST_F(CodeImmuneIntegratedSelfRepairTest, GetAntigenDiagnosticNullSystem) {
    diagnostic_result_t diag;
    int ret = code_immune_get_antigen_diagnostic(NULL, 1, &diag);
    EXPECT_EQ(ret, -1);
}

TEST_F(CodeImmuneIntegratedSelfRepairTest, GetAntigenDiagnosticNullOutput) {
    ASSERT_NE(immune, nullptr);

    uint64_t antigen_id = present_test_crash(SIGSEGV);
    ASSERT_NE(antigen_id, 0u);

    int ret = code_immune_get_antigen_diagnostic(immune, antigen_id, NULL);
    EXPECT_EQ(ret, -1);
}

TEST_F(CodeImmuneIntegratedSelfRepairTest, DiagnosticSeverityMapping) {
    ASSERT_NE(immune, nullptr);

    // High severity crash
    uint64_t antigen_id = present_test_crash(SIGSEGV);
    ASSERT_NE(antigen_id, 0u);

    diagnostic_result_t diag;
    code_immune_get_antigen_diagnostic(immune, antigen_id, &diag);

    // Severity is based on the signal type, not a configurable float
    // SIGSEGV typically maps to CRITICAL or FATAL
    EXPECT_GE((int)diag.severity, (int)DIAG_SEVERITY_CRITICAL);
}

//=============================================================================
// Auto-Repair Eligibility Tests
//=============================================================================

TEST_F(CodeImmuneIntegratedSelfRepairTest, CheckAutoRepairNotConnected) {
    ASSERT_NE(immune, nullptr);

    uint64_t antigen_id = present_test_crash(SIGSEGV);
    ASSERT_NE(antigen_id, 0u);

    // Not connected to self-repair, should not be eligible
    bool eligible = code_immune_check_auto_repair_eligible(immune, antigen_id);
    EXPECT_FALSE(eligible);
}

TEST_F(CodeImmuneIntegratedSelfRepairTest, CheckAutoRepairBelowThresholds) {
    ASSERT_NE(immune, nullptr);

    // Connect self-repair
    self_repair_coordinator_t* self_repair = self_repair_create(NULL);
    code_immune_connect_self_repair(immune, self_repair);

    // Single crash - below min_crash_count threshold (3)
    uint64_t antigen_id = present_test_crash(SIGSEGV);
    ASSERT_NE(antigen_id, 0u);

    bool eligible = code_immune_check_auto_repair_eligible(immune, antigen_id);
    EXPECT_FALSE(eligible);  // Only 1 crash, need 3

    code_immune_disconnect_self_repair(immune);
    self_repair_destroy(self_repair);
}

TEST_F(CodeImmuneIntegratedSelfRepairTest, CheckAutoRepairNullSystem) {
    bool eligible = code_immune_check_auto_repair_eligible(NULL, 1);
    EXPECT_FALSE(eligible);
}

TEST_F(CodeImmuneIntegratedSelfRepairTest, CheckAutoRepairInvalidAntigen) {
    ASSERT_NE(immune, nullptr);

    self_repair_coordinator_t* self_repair = self_repair_create(NULL);
    code_immune_connect_self_repair(immune, self_repair);

    bool eligible = code_immune_check_auto_repair_eligible(immune, 99999);
    EXPECT_FALSE(eligible);

    code_immune_disconnect_self_repair(immune);
    self_repair_destroy(self_repair);
}

//=============================================================================
// Repair Outcome Handling Tests
//=============================================================================

TEST_F(CodeImmuneIntegratedSelfRepairTest, HandleRepairOutcomeSuccess) {
    ASSERT_NE(immune, nullptr);

    uint64_t antigen_id = present_test_crash(SIGSEGV);
    ASSERT_NE(antigen_id, 0u);

    code_immune_repair_outcome_t outcome;
    memset(&outcome, 0, sizeof(outcome));
    outcome.antigen_id = antigen_id;
    outcome.repair_id = 12345;
    outcome.success = true;
    outcome.hot_patched = true;
    outcome.source_committed = false;
    outcome.fix_confidence = 0.85f;

    int ret = code_immune_handle_repair_outcome(immune, &outcome);
    EXPECT_EQ(ret, 0);

    // Check statistics updated
    uint64_t triggered, successful, failed;
    code_immune_get_repair_stats(immune, &triggered, &successful, &failed);
    EXPECT_EQ(successful, 1u);
}

TEST_F(CodeImmuneIntegratedSelfRepairTest, HandleRepairOutcomeFailure) {
    ASSERT_NE(immune, nullptr);

    uint64_t antigen_id = present_test_crash(SIGSEGV);
    ASSERT_NE(antigen_id, 0u);

    code_immune_repair_outcome_t outcome;
    memset(&outcome, 0, sizeof(outcome));
    outcome.antigen_id = antigen_id;
    outcome.repair_id = 12345;
    outcome.success = false;
    strncpy(outcome.error_message, "Validation failed",
            sizeof(outcome.error_message) - 1);

    int ret = code_immune_handle_repair_outcome(immune, &outcome);
    EXPECT_EQ(ret, 0);

    // Check statistics updated
    uint64_t triggered, successful, failed;
    code_immune_get_repair_stats(immune, &triggered, &successful, &failed);
    EXPECT_EQ(failed, 1u);
}

TEST_F(CodeImmuneIntegratedSelfRepairTest, HandleRepairOutcomeNullSystem) {
    code_immune_repair_outcome_t outcome;
    memset(&outcome, 0, sizeof(outcome));

    int ret = code_immune_handle_repair_outcome(NULL, &outcome);
    EXPECT_EQ(ret, -1);
}

TEST_F(CodeImmuneIntegratedSelfRepairTest, HandleRepairOutcomeNullOutcome) {
    ASSERT_NE(immune, nullptr);

    int ret = code_immune_handle_repair_outcome(immune, NULL);
    EXPECT_EQ(ret, -1);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(CodeImmuneIntegratedSelfRepairTest, GetRepairStatsInitiallyZero) {
    ASSERT_NE(immune, nullptr);

    uint64_t triggered, successful, failed;
    int ret = code_immune_get_repair_stats(immune, &triggered, &successful, &failed);

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(triggered, 0u);
    EXPECT_EQ(successful, 0u);
    EXPECT_EQ(failed, 0u);
}

TEST_F(CodeImmuneIntegratedSelfRepairTest, GetRepairStatsPartialOutput) {
    ASSERT_NE(immune, nullptr);

    // Should work with partial NULL outputs
    uint64_t triggered;
    int ret = code_immune_get_repair_stats(immune, &triggered, NULL, NULL);

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(triggered, 0u);
}

TEST_F(CodeImmuneIntegratedSelfRepairTest, GetRepairStatsNullSystem) {
    uint64_t triggered;
    int ret = code_immune_get_repair_stats(NULL, &triggered, NULL, NULL);
    EXPECT_EQ(ret, -1);
}

//=============================================================================
// Trigger Repair Tests
//=============================================================================

TEST_F(CodeImmuneIntegratedSelfRepairTest, TriggerRepairNotConnected) {
    ASSERT_NE(immune, nullptr);

    uint64_t antigen_id = present_test_crash(SIGSEGV);
    ASSERT_NE(antigen_id, 0u);

    uint64_t repair_id;
    int ret = code_immune_trigger_repair(immune, antigen_id, &repair_id);

    // Should fail - not connected
    EXPECT_EQ(ret, -1);
}

TEST_F(CodeImmuneIntegratedSelfRepairTest, TriggerRepairNullSystem) {
    uint64_t repair_id;
    int ret = code_immune_trigger_repair(NULL, 1, &repair_id);
    EXPECT_EQ(ret, -1);
}

TEST_F(CodeImmuneIntegratedSelfRepairTest, TriggerRepairInvalidAntigen) {
    ASSERT_NE(immune, nullptr);

    self_repair_coordinator_t* self_repair = self_repair_create(NULL);
    code_immune_connect_self_repair(immune, self_repair);

    uint64_t repair_id;
    int ret = code_immune_trigger_repair(immune, 99999, &repair_id);

    EXPECT_EQ(ret, -1);  // Antigen not found

    code_immune_disconnect_self_repair(immune);
    self_repair_destroy(self_repair);
}

//=============================================================================
// Integration with Update Cycle Tests
//=============================================================================

TEST_F(CodeImmuneIntegratedSelfRepairTest, UpdateCycleWithAutoRepairDisabled) {
    ASSERT_NE(immune, nullptr);

    // Disable auto-repair
    immune->config.auto_repair.enabled = false;

    // Present crash
    present_test_crash(SIGSEGV);

    // Update should not crash
    int ret = code_immune_update(immune, 100);  // 100ms delta
    EXPECT_EQ(ret, 0);
}

TEST_F(CodeImmuneIntegratedSelfRepairTest, UpdateCycleNotConnected) {
    ASSERT_NE(immune, nullptr);

    // Auto-repair enabled but not connected
    immune->config.auto_repair.enabled = true;

    // Present crash
    present_test_crash(SIGSEGV);

    // Update should not crash - just skip auto-repair checks
    int ret = code_immune_update(immune, 100);  // 100ms delta
    EXPECT_EQ(ret, 0);
}
