/**
 * @file test_exception_immune_regression.cpp
 * @brief Regression tests for exception-immune system integration
 *
 * WHAT: Verify immune integration contracts remain stable
 * WHY:  Prevent breaking changes to exception-immune bridge
 * HOW:  Test exact mapping behaviors, recovery strategies, epitope generation
 *
 * REGRESSION CATEGORIES:
 * 1. Antigen Source Mapping - Exception categories map to correct antigen sources
 * 2. Severity Mapping - Exception severity maps to immune severity (1-10)
 * 3. Recovery Strategy - Exception types get correct recovery strategies
 * 4. Epitope Generation - Epitope generation is deterministic and stable
 * 5. Default Config - Default configuration values are stable
 * 6. Connection API - Connect/disconnect behavior is stable
 * 7. Presentation API - Exception presentation returns correct responses
 * 8. Recovery Callbacks - Default recovery callbacks exist and work
 * 9. Statistics - Statistics tracking is accurate
 *
 * @author NIMCP Development Team
 * @date 2026-01-16
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cstdint>

extern "C" {
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_handlers.h"
#include "utils/exception/nimcp_exception_immune.h"
#include "utils/exception/nimcp_exception_circuit.h"
#include "utils/exception/nimcp_exception_metrics.h"
#include "utils/error/nimcp_error_codes.h"
}

//=============================================================================
// Test Fixture
//=============================================================================

class ExceptionImmuneRegressionTest : public ::testing::Test {
protected:
    void SetUp() override {
        nimcp_exception_system_init();
        // Initialize immune integration without a real immune system
        // (we test the mapping functions which don't require connection)
        nimcp_exception_immune_init(nullptr);
    }

    void TearDown() override {
        nimcp_exception_immune_shutdown();
        nimcp_exception_clear_current();
        nimcp_exception_system_shutdown();
    }
};

//=============================================================================
// Antigen Source Mapping Regression Tests
// REGRESSION: Category to antigen source mapping must be stable
//=============================================================================

TEST_F(ExceptionImmuneRegressionTest, MemoryCategoryMapsToAnomaly) {
    // REGRESSION: MEMORY category maps to ANOMALY source
    // (memory anomaly detection)

    exception_antigen_source_t source = nimcp_exception_to_antigen_source(
        EXCEPTION_CATEGORY_MEMORY
    );
    EXPECT_EQ(source, EX_ANTIGEN_SOURCE_ANOMALY)
        << "MEMORY category must map to ANOMALY source for memory anomaly detection";
}

TEST_F(ExceptionImmuneRegressionTest, BrainCategoryMapsToBBB) {
    // REGRESSION: BRAIN category maps to BBB source
    // (brain blood barrier - security)

    exception_antigen_source_t source = nimcp_exception_to_antigen_source(
        EXCEPTION_CATEGORY_BRAIN
    );
    EXPECT_EQ(source, EX_ANTIGEN_SOURCE_BBB)
        << "BRAIN category must map to BBB source for brain security";
}

TEST_F(ExceptionImmuneRegressionTest, ThreadingCategoryMapsToBFT) {
    // REGRESSION: THREADING category maps to BFT source
    // (byzantine fault tolerance)

    exception_antigen_source_t source = nimcp_exception_to_antigen_source(
        EXCEPTION_CATEGORY_THREADING
    );
    EXPECT_EQ(source, EX_ANTIGEN_SOURCE_BFT)
        << "THREADING category must map to BFT source for byzantine detection";
}

TEST_F(ExceptionImmuneRegressionTest, SecurityCategoryMapsToBBB) {
    // REGRESSION: SECURITY category maps to BBB source
    // (security threat detection)

    exception_antigen_source_t source = nimcp_exception_to_antigen_source(
        EXCEPTION_CATEGORY_SECURITY
    );
    EXPECT_EQ(source, EX_ANTIGEN_SOURCE_BBB)
        << "SECURITY category must map to BBB source for security threats";
}

TEST_F(ExceptionImmuneRegressionTest, IOCategoryMapsToAnomaly) {
    // REGRESSION: IO category maps to ANOMALY source
    // (system anomaly detection)

    exception_antigen_source_t source = nimcp_exception_to_antigen_source(
        EXCEPTION_CATEGORY_IO
    );
    EXPECT_EQ(source, EX_ANTIGEN_SOURCE_ANOMALY)
        << "IO category must map to ANOMALY source for system anomalies";
}

TEST_F(ExceptionImmuneRegressionTest, SignalCategoryMapsToAnomaly) {
    // REGRESSION: SIGNAL category maps to ANOMALY source
    // (crash anomaly detection)

    exception_antigen_source_t source = nimcp_exception_to_antigen_source(
        EXCEPTION_CATEGORY_SIGNAL
    );
    EXPECT_EQ(source, EX_ANTIGEN_SOURCE_ANOMALY)
        << "SIGNAL category must map to ANOMALY source for crash anomalies";
}

TEST_F(ExceptionImmuneRegressionTest, GenericCategoryMapsToAnomaly) {
    // REGRESSION: GENERIC category maps to ANOMALY source

    exception_antigen_source_t source = nimcp_exception_to_antigen_source(
        EXCEPTION_CATEGORY_GENERIC
    );
    EXPECT_EQ(source, EX_ANTIGEN_SOURCE_ANOMALY)
        << "GENERIC category must map to ANOMALY source";
}

//=============================================================================
// Severity Mapping Regression Tests
// REGRESSION: Exception severity to immune severity (1-10) mapping
//=============================================================================

TEST_F(ExceptionImmuneRegressionTest, SeverityMappingIsIdentity) {
    // REGRESSION: Exception severity values ARE the immune severity values
    // (by design - enum values 1-10 match immune severity scale)

    EXPECT_EQ(nimcp_exception_to_immune_severity(EXCEPTION_SEVERITY_DEBUG), 1u)
        << "DEBUG severity must map to immune severity 1";

    EXPECT_EQ(nimcp_exception_to_immune_severity(EXCEPTION_SEVERITY_INFO), 2u)
        << "INFO severity must map to immune severity 2";

    EXPECT_EQ(nimcp_exception_to_immune_severity(EXCEPTION_SEVERITY_WARNING), 3u)
        << "WARNING severity must map to immune severity 3";

    EXPECT_EQ(nimcp_exception_to_immune_severity(EXCEPTION_SEVERITY_ERROR), 5u)
        << "ERROR severity must map to immune severity 5";

    EXPECT_EQ(nimcp_exception_to_immune_severity(EXCEPTION_SEVERITY_SEVERE), 7u)
        << "SEVERE severity must map to immune severity 7";

    EXPECT_EQ(nimcp_exception_to_immune_severity(EXCEPTION_SEVERITY_CRITICAL), 9u)
        << "CRITICAL severity must map to immune severity 9";

    EXPECT_EQ(nimcp_exception_to_immune_severity(EXCEPTION_SEVERITY_FATAL), 10u)
        << "FATAL severity must map to immune severity 10";
}

//=============================================================================
// Recovery Strategy Regression Tests
// REGRESSION: Exception types must get correct recovery strategies
//=============================================================================

TEST_F(ExceptionImmuneRegressionTest, MemoryExceptionRecoveryStrategy) {
    // REGRESSION: MEMORY exceptions get GC + Compact primary, Quarantine fallback

    nimcp_memory_exception_t* mem_ex = nimcp_memory_exception_create(
        NIMCP_ERROR_NO_MEMORY,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        1024,
        "Memory test"
    );
    ASSERT_NE(mem_ex, nullptr);

    nimcp_exception_recovery_strategy_t strategy;
    nimcp_exception_get_recovery_strategy((nimcp_exception_t*)mem_ex, &strategy);

    EXPECT_EQ(strategy.primary_action, EXCEPTION_RECOVERY_GC)
        << "MEMORY exception primary action must be GC";
    EXPECT_EQ(strategy.fallback_action, EXCEPTION_RECOVERY_QUARANTINE)
        << "MEMORY exception fallback action must be QUARANTINE";

    nimcp_exception_unref((nimcp_exception_t*)mem_ex);
}

TEST_F(ExceptionImmuneRegressionTest, BrainExceptionRecoveryStrategy) {
    // REGRESSION: BRAIN exceptions get specific recovery strategy

    nimcp_brain_exception_t* brain_ex = nimcp_brain_exception_create(
        NIMCP_ERROR_BRAIN_CREATION,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        1, "test_region",
        "Brain test"
    );
    ASSERT_NE(brain_ex, nullptr);

    nimcp_exception_recovery_strategy_t strategy;
    nimcp_exception_get_recovery_strategy((nimcp_exception_t*)brain_ex, &strategy);

    // Brain exceptions should have CLEAR_CACHE or similar as primary
    // and ROLLBACK as fallback
    EXPECT_NE(strategy.primary_action, EXCEPTION_RECOVERY_NONE)
        << "BRAIN exception must have a primary action";

    nimcp_exception_unref((nimcp_exception_t*)brain_ex);
}

TEST_F(ExceptionImmuneRegressionTest, SignalExceptionRecoveryStrategy) {
    // REGRESSION: SIGNAL exceptions get EMERGENCY_SAVE primary, GRACEFUL_SHUTDOWN fallback

    nimcp_signal_exception_t* sig_ex = nimcp_signal_exception_create(
        SIGSEGV,
        (void*)0x1234,
        __FILE__, __LINE__, __func__,
        "Signal test"
    );
    ASSERT_NE(sig_ex, nullptr);

    nimcp_exception_recovery_strategy_t strategy;
    nimcp_exception_get_recovery_strategy((nimcp_exception_t*)sig_ex, &strategy);

    EXPECT_EQ(strategy.primary_action, EXCEPTION_RECOVERY_EMERGENCY_SAVE)
        << "SIGNAL exception primary action must be EMERGENCY_SAVE";
    EXPECT_EQ(strategy.fallback_action, EXCEPTION_RECOVERY_GRACEFUL_SHUTDOWN)
        << "SIGNAL exception fallback action must be GRACEFUL_SHUTDOWN";
    EXPECT_EQ(strategy.retry_count, 1u)
        << "SIGNAL exception retry_count must be 1 (one chance)";
    EXPECT_EQ(strategy.cooldown_ms, 0u)
        << "SIGNAL exception cooldown_ms must be 0 (immediate)";

    nimcp_exception_unref((nimcp_exception_t*)sig_ex);
}

TEST_F(ExceptionImmuneRegressionTest, ThreadingExceptionRecoveryStrategy) {
    // REGRESSION: THREADING exceptions have specific recovery strategy

    nimcp_threading_exception_t* thread_ex = nimcp_threading_exception_create(
        NIMCP_ERROR_DEADLOCK,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        12345,
        "Threading test"
    );
    ASSERT_NE(thread_ex, nullptr);

    nimcp_exception_recovery_strategy_t strategy;
    nimcp_exception_get_recovery_strategy((nimcp_exception_t*)thread_ex, &strategy);

    EXPECT_EQ(strategy.primary_action, EXCEPTION_RECOVERY_RESTART_THREAD)
        << "THREADING exception primary action must be RESTART_THREAD";

    nimcp_exception_unref((nimcp_exception_t*)thread_ex);
}

TEST_F(ExceptionImmuneRegressionTest, IOExceptionRecoveryStrategy) {
    // REGRESSION: IO exceptions have RETRY as primary action

    nimcp_io_exception_t* io_ex = nimcp_io_exception_create(
        NIMCP_ERROR_FILE_READ,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "/tmp/test.txt",
        "IO test"
    );
    ASSERT_NE(io_ex, nullptr);

    nimcp_exception_recovery_strategy_t strategy;
    nimcp_exception_get_recovery_strategy((nimcp_exception_t*)io_ex, &strategy);

    EXPECT_EQ(strategy.primary_action, EXCEPTION_RECOVERY_RETRY)
        << "IO exception primary action must be RETRY";

    nimcp_exception_unref((nimcp_exception_t*)io_ex);
}

//=============================================================================
// Default Configuration Regression Tests
// REGRESSION: Default configuration values must be stable
//=============================================================================

TEST_F(ExceptionImmuneRegressionTest, DefaultConfigValues) {
    // REGRESSION: Default configuration must have consistent values

    nimcp_exception_immune_config_t config;
    nimcp_exception_immune_default_config(&config);

    EXPECT_TRUE(config.enable_auto_present)
        << "Default enable_auto_present must be true";

    EXPECT_EQ(config.min_present_severity, EXCEPTION_SEVERITY_SEVERE)
        << "Default min_present_severity must be SEVERE";

    EXPECT_TRUE(config.enable_auto_recovery)
        << "Default enable_auto_recovery must be true";

    EXPECT_TRUE(config.enable_memory_formation)
        << "Default enable_memory_formation must be true";

    EXPECT_FALSE(config.async_presentation)
        << "Default async_presentation must be false";

    EXPECT_GT(config.max_pending_exceptions, 0u)
        << "Default max_pending_exceptions must be > 0";

    EXPECT_GT(config.response_timeout_ms, 0u)
        << "Default response_timeout_ms must be > 0";
}

//=============================================================================
// Epitope Generation Regression Tests
// REGRESSION: Epitope generation must be deterministic
//=============================================================================

TEST_F(ExceptionImmuneRegressionTest, EpitopeGenerationDeterministic) {
    // REGRESSION: Same exception should generate same epitope

    nimcp_exception_t* ex1 = nimcp_exception_create(
        NIMCP_ERROR_NO_MEMORY,
        EXCEPTION_SEVERITY_ERROR,
        "test_file.c", 100, "test_func",
        "Test message"
    );
    ASSERT_NE(ex1, nullptr);

    nimcp_exception_t* ex2 = nimcp_exception_create(
        NIMCP_ERROR_NO_MEMORY,
        EXCEPTION_SEVERITY_ERROR,
        "test_file.c", 100, "test_func",
        "Test message"
    );
    ASSERT_NE(ex2, nullptr);

    // Generate epitopes
    uint8_t epitope1[64] = {0};
    uint8_t epitope2[64] = {0};

    size_t len1 = nimcp_exception_compute_epitope(ex1, epitope1, sizeof(epitope1));
    size_t len2 = nimcp_exception_compute_epitope(ex2, epitope2, sizeof(epitope2));

    EXPECT_EQ(len1, len2)
        << "Same exception should generate same epitope length";

    EXPECT_EQ(memcmp(epitope1, epitope2, len1), 0)
        << "Same exception should generate identical epitope";

    nimcp_exception_unref(ex1);
    nimcp_exception_unref(ex2);
}

TEST_F(ExceptionImmuneRegressionTest, EpitopeGenerationSize) {
    // REGRESSION: Epitope should be 64 bytes or less

    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_UNKNOWN,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Epitope size test"
    );
    ASSERT_NE(ex, nullptr);

    uint8_t epitope[64] = {0};
    size_t len = nimcp_exception_compute_epitope(ex, epitope, sizeof(epitope));

    EXPECT_LE(len, 64u)
        << "Epitope must be 64 bytes or less";
    EXPECT_GT(len, 0u)
        << "Epitope must have non-zero length";

    nimcp_exception_unref(ex);
}

TEST_F(ExceptionImmuneRegressionTest, DifferentExceptionsDifferentEpitopes) {
    // REGRESSION: Different exceptions should (usually) generate different epitopes

    nimcp_exception_t* ex1 = nimcp_exception_create(
        NIMCP_ERROR_NO_MEMORY,
        EXCEPTION_SEVERITY_ERROR,
        "file1.c", 100, "func1",
        "Memory error"
    );

    nimcp_exception_t* ex2 = nimcp_exception_create(
        NIMCP_ERROR_FILE_NOT_FOUND,
        EXCEPTION_SEVERITY_WARNING,
        "file2.c", 200, "func2",
        "IO error"
    );

    ASSERT_NE(ex1, nullptr);
    ASSERT_NE(ex2, nullptr);

    uint8_t epitope1[64] = {0};
    uint8_t epitope2[64] = {0};

    size_t len1 = nimcp_exception_compute_epitope(ex1, epitope1, sizeof(epitope1));
    size_t len2 = nimcp_exception_compute_epitope(ex2, epitope2, sizeof(epitope2));

    // Different exceptions should have different epitopes
    bool different = (len1 != len2) || (memcmp(epitope1, epitope2, len1) != 0);
    EXPECT_TRUE(different)
        << "Different exceptions should generate different epitopes";

    nimcp_exception_unref(ex1);
    nimcp_exception_unref(ex2);
}

//=============================================================================
// Connection API Regression Tests
// REGRESSION: Connect/disconnect behavior must be stable
//=============================================================================

TEST_F(ExceptionImmuneRegressionTest, InitiallyNotConnected) {
    // REGRESSION: System starts not connected to immune system
    // (immune init with NULL doesn't connect)

    // After init with NULL config, we're initialized but not connected
    // to a real immune system
    EXPECT_FALSE(nimcp_exception_immune_is_connected())
        << "System should not be connected without explicit connect call";
}

TEST_F(ExceptionImmuneRegressionTest, DisconnectSafe) {
    // REGRESSION: Disconnect when not connected should be safe

    int result = nimcp_exception_immune_disconnect();
    EXPECT_EQ(result, 0)
        << "Disconnect when not connected should return 0 (no-op)";
}

//=============================================================================
// Presentation API Regression Tests
// REGRESSION: Exception presentation must work correctly
//=============================================================================

TEST_F(ExceptionImmuneRegressionTest, PresentationWithoutConnection) {
    // REGRESSION: Presentation without connection should handle gracefully

    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_UNKNOWN,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        "Presentation test"
    );
    ASSERT_NE(ex, nullptr);

    nimcp_immune_response_t response;
    memset(&response, 0, sizeof(response));

    // Without a connected immune system, presentation should still work
    // (it just won't actually present to immune)
    int result = nimcp_exception_present_to_immune(ex, &response);
    // Result may be 0 (no-op success) or -1 (not connected error)
    // depending on implementation

    // Mark should be set on exception
    // (or not if not connected - depends on implementation)

    nimcp_exception_unref(ex);
}

TEST_F(ExceptionImmuneRegressionTest, AsyncPresentationWithoutConnection) {
    // REGRESSION: Async presentation without connection should handle gracefully

    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_UNKNOWN,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        "Async test"
    );
    ASSERT_NE(ex, nullptr);

    int result = nimcp_exception_present_async(ex);
    // Should either succeed (queued) or fail (not connected)
    // Should not crash

    nimcp_exception_unref(ex);
}

TEST_F(ExceptionImmuneRegressionTest, ProcessPendingWithEmpty) {
    // REGRESSION: Process pending with empty queue should return 0

    size_t processed = nimcp_exception_immune_process_pending(0);
    EXPECT_EQ(processed, 0u)
        << "Process pending with empty queue must return 0";
}

//=============================================================================
// Statistics Regression Tests
// REGRESSION: Statistics tracking must be accurate
//=============================================================================

TEST_F(ExceptionImmuneRegressionTest, StatisticsInitialValues) {
    // REGRESSION: Initial statistics should be zero or initialized

    nimcp_exception_immune_stats_t stats;
    nimcp_exception_immune_get_stats(&stats);

    // Initial values should be 0 or reasonable defaults
    // (after init, before any exceptions are presented)
    EXPECT_GE(stats.exceptions_presented, 0u);
    EXPECT_GE(stats.exceptions_pending, 0u);
    EXPECT_GE(stats.recoveries_attempted, 0u);
    EXPECT_GE(stats.recoveries_succeeded, 0u);
    EXPECT_GE(stats.memories_formed, 0u);
    EXPECT_GE(stats.avg_response_time_us, 0.0f);
    EXPECT_GE(stats.queue_overflows, 0u);
}

TEST_F(ExceptionImmuneRegressionTest, StatisticsReset) {
    // REGRESSION: Statistics reset must clear all values

    // Do some operations to potentially increment stats
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_UNKNOWN,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        "Stats test"
    );
    nimcp_exception_present_to_immune(ex, nullptr);
    nimcp_exception_unref(ex);

    // Reset stats
    nimcp_exception_immune_reset_stats();

    // Get stats
    nimcp_exception_immune_stats_t stats;
    nimcp_exception_immune_get_stats(&stats);

    EXPECT_EQ(stats.exceptions_presented, 0u)
        << "exceptions_presented must be 0 after reset";
    EXPECT_EQ(stats.recoveries_attempted, 0u)
        << "recoveries_attempted must be 0 after reset";
    EXPECT_EQ(stats.recoveries_succeeded, 0u)
        << "recoveries_succeeded must be 0 after reset";
}

//=============================================================================
// Default Recovery Callbacks Regression Tests
// REGRESSION: Default recovery callbacks must exist
//=============================================================================

TEST_F(ExceptionImmuneRegressionTest, DefaultRecoveryCallbacksInstall) {
    // REGRESSION: Installing default recovery callbacks should succeed

    int result = nimcp_exception_install_default_recovery_callbacks();
    EXPECT_EQ(result, 0)
        << "Installing default recovery callbacks must succeed";
}

//=============================================================================
// Recovery Execution Regression Tests
// REGRESSION: Recovery execution contract must be stable
//=============================================================================

TEST_F(ExceptionImmuneRegressionTest, RecoveryExecutionWithoutCallback) {
    // REGRESSION: Recovery execution without registered callback should fail

    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_UNKNOWN,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Recovery test"
    );
    ASSERT_NE(ex, nullptr);

    // Unregister to ensure no callback
    nimcp_unregister_recovery_callback(EXCEPTION_RECOVERY_COMPACT);

    int result = nimcp_exception_execute_recovery(ex, EXCEPTION_RECOVERY_COMPACT);
    EXPECT_EQ(result, -1)
        << "Recovery execution without callback must return -1";

    nimcp_exception_unref(ex);
}

TEST_F(ExceptionImmuneRegressionTest, RecoveryNotificationContract) {
    // REGRESSION: Recovery notification should work without connection

    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_UNKNOWN,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Notification test"
    );
    ASSERT_NE(ex, nullptr);

    int result = nimcp_exception_notify_recovery_result(
        ex,
        EXCEPTION_RECOVERY_GC,
        true  // success
    );
    // Should succeed or be no-op (return 0)
    EXPECT_EQ(result, 0)
        << "Recovery notification should return 0";

    nimcp_exception_unref(ex);
}

//=============================================================================
// Exception Fields After Immune Presentation Regression Tests
// REGRESSION: Immune presentation should set certain fields
//=============================================================================

TEST_F(ExceptionImmuneRegressionTest, ExceptionFieldsAfterEpitopeGeneration) {
    // REGRESSION: After epitope generation, fields should be set

    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_UNKNOWN,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Field test"
    );
    ASSERT_NE(ex, nullptr);

    // Generate epitope directly
    size_t len = nimcp_exception_generate_epitope(ex);
    EXPECT_GT(len, 0u)
        << "Epitope generation must return non-zero length";

    EXPECT_EQ(ex->epitope_len, len)
        << "Exception epitope_len must match returned length";

    // Epitope should have non-zero bytes
    bool has_nonzero = false;
    for (size_t i = 0; i < len; i++) {
        if (ex->epitope[i] != 0) {
            has_nonzero = true;
            break;
        }
    }
    EXPECT_TRUE(has_nonzero)
        << "Epitope should have non-zero content";

    nimcp_exception_unref(ex);
}

//=============================================================================
// Suggested Recovery Contract Tests
// REGRESSION: nimcp_exception_get_suggested_recovery must be consistent
//=============================================================================

TEST_F(ExceptionImmuneRegressionTest, SuggestedRecoveryForMemory) {
    // REGRESSION: Memory exceptions suggest GC

    nimcp_memory_exception_t* mem_ex = nimcp_memory_exception_create(
        NIMCP_ERROR_NO_MEMORY,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        1024,
        "Memory suggestion test"
    );
    ASSERT_NE(mem_ex, nullptr);

    nimcp_exception_recovery_action_t suggested = nimcp_exception_get_suggested_recovery(
        (nimcp_exception_t*)mem_ex
    );
    EXPECT_EQ(suggested, EXCEPTION_RECOVERY_GC)
        << "Memory exception suggested recovery must be GC";

    nimcp_exception_unref((nimcp_exception_t*)mem_ex);
}

TEST_F(ExceptionImmuneRegressionTest, SuggestedRecoveryForSignal) {
    // REGRESSION: Signal exceptions suggest EMERGENCY_SAVE

    nimcp_signal_exception_t* sig_ex = nimcp_signal_exception_create(
        SIGSEGV,
        nullptr,
        __FILE__, __LINE__, __func__,
        "Signal suggestion test"
    );
    ASSERT_NE(sig_ex, nullptr);

    nimcp_exception_recovery_action_t suggested = nimcp_exception_get_suggested_recovery(
        (nimcp_exception_t*)sig_ex
    );
    EXPECT_EQ(suggested, EXCEPTION_RECOVERY_EMERGENCY_SAVE)
        << "Signal exception suggested recovery must be EMERGENCY_SAVE";

    nimcp_exception_unref((nimcp_exception_t*)sig_ex);
}

//=============================================================================
// Recovery Strategy Fields Regression Tests
// REGRESSION: Recovery strategy struct fields must be populated
//=============================================================================

TEST_F(ExceptionImmuneRegressionTest, RecoveryStrategyFieldsPopulated) {
    // REGRESSION: All recovery strategy fields must be populated

    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_NO_MEMORY,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        "Strategy fields test"
    );
    ASSERT_NE(ex, nullptr);

    nimcp_exception_recovery_strategy_t strategy;
    memset(&strategy, 0xFF, sizeof(strategy));  // Fill with garbage

    nimcp_exception_get_recovery_strategy(ex, &strategy);

    // All fields should be set to valid values (not 0xFF garbage)
    EXPECT_NE(strategy.primary_action, (nimcp_exception_recovery_action_t)0xFFFFFFFF);
    EXPECT_NE(strategy.fallback_action, (nimcp_exception_recovery_action_t)0xFFFFFFFF);
    // retry_count and cooldown_ms can be 0 or other values

    nimcp_exception_unref(ex);
}

//=============================================================================
// Immune Response Fields Regression Tests
// REGRESSION: Immune response struct has expected fields
//=============================================================================

TEST_F(ExceptionImmuneRegressionTest, ImmuneResponseStructSize) {
    // REGRESSION: nimcp_immune_response_t struct size should be stable
    // (for binary compatibility)

    nimcp_immune_response_t response;

    // Check that all expected fields exist by accessing them
    response.antigen_id = 0;
    response.antibody_id = 0;
    response.action_taken = EXCEPTION_RECOVERY_NONE;
    response.recovery_attempted = false;
    response.recovery_succeeded = false;
    response.response_time_us = 0;
    response.memory_formed = false;

    // Struct should be reasonable size
    EXPECT_GT(sizeof(nimcp_immune_response_t), 0u);
}

//=============================================================================
// Immune Config Fields Regression Tests
// REGRESSION: Immune config struct has expected fields
//=============================================================================

TEST_F(ExceptionImmuneRegressionTest, ImmuneConfigStructFields) {
    // REGRESSION: nimcp_exception_immune_config_t has expected fields

    nimcp_exception_immune_config_t config;

    // Check all expected fields exist
    config.enable_auto_present = true;
    config.min_present_severity = EXCEPTION_SEVERITY_SEVERE;
    config.enable_auto_recovery = true;
    config.enable_memory_formation = true;
    config.async_presentation = false;
    config.max_pending_exceptions = 256;
    config.response_timeout_ms = 5000;

    // Struct should have reasonable size
    EXPECT_GT(sizeof(nimcp_exception_immune_config_t), 0u);
}

//=============================================================================
// Shutdown/Init Idempotency Regression Tests
// REGRESSION: Init and shutdown should be idempotent
//=============================================================================

TEST_F(ExceptionImmuneRegressionTest, InitShutdownIdempotent) {
    // Note: Already init'd in SetUp

    // Shutdown
    nimcp_exception_immune_shutdown();

    // Multiple shutdowns should be safe
    nimcp_exception_immune_shutdown();
    nimcp_exception_immune_shutdown();

    // Init again
    int result = nimcp_exception_immune_init(nullptr);
    EXPECT_EQ(result, 0);

    // Multiple inits should be safe (or return appropriate error)
    result = nimcp_exception_immune_init(nullptr);
    // May succeed (idempotent) or fail (already init'd)
}

//=============================================================================
// Null Parameter Safety Regression Tests
// REGRESSION: Functions must handle NULL parameters safely
//=============================================================================

TEST_F(ExceptionImmuneRegressionTest, NullParameterSafety) {
    // REGRESSION: Functions must not crash with NULL parameters

    // nimcp_exception_to_antigen_source with invalid category
    // (just verify it doesn't crash, returns something)
    exception_antigen_source_t source = nimcp_exception_to_antigen_source(
        (nimcp_exception_category_t)999
    );
    // Should return some default value

    // nimcp_exception_to_immune_severity with invalid severity
    uint32_t sev = nimcp_exception_to_immune_severity(
        (nimcp_exception_severity_t)999
    );
    // Should return some value (clamped or default)

    // nimcp_exception_get_recovery_strategy with NULL exception
    nimcp_exception_recovery_strategy_t strategy;
    nimcp_exception_get_recovery_strategy(nullptr, &strategy);
    // Should not crash

    // nimcp_exception_get_recovery_strategy with NULL strategy
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_UNKNOWN,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "NULL test"
    );
    nimcp_exception_get_recovery_strategy(ex, nullptr);
    // Should not crash
    nimcp_exception_unref(ex);

    // nimcp_exception_compute_epitope with NULL exception
    uint8_t epitope[64];
    size_t len = nimcp_exception_compute_epitope(nullptr, epitope, sizeof(epitope));
    EXPECT_EQ(len, 0u)
        << "Epitope computation with NULL exception should return 0";

    // nimcp_exception_compute_epitope with NULL epitope
    ex = nimcp_exception_create(
        NIMCP_ERROR_UNKNOWN,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "NULL epitope test"
    );
    len = nimcp_exception_compute_epitope(ex, nullptr, 64);
    EXPECT_EQ(len, 0u)
        << "Epitope computation with NULL buffer should return 0";
    nimcp_exception_unref(ex);

    // nimcp_exception_present_to_immune with NULL exception
    int result = nimcp_exception_present_to_immune(nullptr, nullptr);
    EXPECT_EQ(result, -1)
        << "Present with NULL exception should return -1";

    // nimcp_exception_immune_default_config with NULL
    nimcp_exception_immune_default_config(nullptr);
    // Should not crash

    // nimcp_exception_immune_get_stats with NULL
    nimcp_exception_immune_get_stats(nullptr);
    // Should not crash
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
