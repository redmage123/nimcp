/**
 * @file test_exception_immune_integration.cpp
 * @brief Integration tests for exception-immune system integration
 * @version 1.0.0
 * @date 2026-01-20
 *
 * WHAT: Test complete integration between exception handling and brain immune system
 * WHY:  Verify exceptions are correctly presented as antigens and immune responses work
 * HOW:  Test exception presentation, epitope generation, recovery strategies,
 *       async processing, and statistics tracking
 *
 * TEST SCENARIOS:
 * - Memory exception presentation to immune system
 * - Brain exception presentation to immune system
 * - Signal exception presentation to immune system
 * - Recovery strategy selection based on exception type
 * - Epitope generation consistency
 * - Async exception presentation and processing
 * - Statistics tracking
 *
 * @author NIMCP Development Team
 */

#include <gtest/gtest.h>
#include <cstring>
#include <thread>
#include <chrono>
#include <atomic>
#include <vector>

// Headers have their own extern "C" guards
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_immune.h"
#include "utils/error/nimcp_error_codes.h"
#include "cognitive/immune/nimcp_brain_immune.h"

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class ExceptionImmuneIntegrationTest : public ::testing::Test {
protected:
    brain_immune_system_t* immune_system = nullptr;
    brain_immune_config_t immune_config;

    void SetUp() override {
        // Initialize exception system
        nimcp_exception_system_init();

        // Initialize exception-immune integration
        nimcp_exception_immune_config_t ex_config;
        nimcp_exception_immune_default_config(&ex_config);
        ex_config.enable_auto_recovery = false;  // Disable auto-recovery for controlled testing
        nimcp_exception_immune_init(&ex_config);

        // Create brain immune system
        brain_immune_default_config(&immune_config);
        immune_system = brain_immune_create(&immune_config);
        ASSERT_NE(immune_system, nullptr);
        brain_immune_start(immune_system);

        // Connect exception system to immune system
        int result = nimcp_exception_immune_connect(immune_system);
        ASSERT_EQ(result, 0);
    }

    void TearDown() override {
        // Disconnect and shutdown
        nimcp_exception_immune_disconnect();

        if (immune_system) {
            brain_immune_stop(immune_system);
            brain_immune_destroy(immune_system);
            immune_system = nullptr;
        }

        nimcp_exception_immune_shutdown();
        nimcp_exception_system_shutdown();
    }

    // Helper to create and validate basic exception
    nimcp_exception_t* createTestException(
        nimcp_error_t code,
        nimcp_exception_severity_t severity,
        const char* message
    ) {
        return nimcp_exception_create(
            code, severity,
            __FILE__, __LINE__, __func__,
            "%s", message
        );
    }
};

/* ============================================================================
 * Memory Exception Presentation Tests
 * ============================================================================ */

TEST_F(ExceptionImmuneIntegrationTest, PresentMemoryExceptionToImmune) {
    // WHAT: Test presenting a memory exception to the immune system
    // WHY:  Verify memory errors trigger correct immune response

    nimcp_memory_exception_t* mem_ex = nimcp_memory_exception_create(
        NIMCP_ERROR_NO_MEMORY,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        1024 * 1024,  // requested_size
        "Failed to allocate 1MB buffer"
    );
    ASSERT_NE(mem_ex, nullptr);

    // Present to immune system
    nimcp_immune_response_t response;
    memset(&response, 0, sizeof(response));
    int result = nimcp_exception_present_to_immune((nimcp_exception_t*)mem_ex, &response);
    EXPECT_EQ(result, 0);

    // Verify response
    EXPECT_GT(response.antigen_id, 0u);
    EXPECT_GE(response.response_time_us, 0u);

    // Verify exception was marked as presented
    EXPECT_TRUE(mem_ex->base.presented_to_immune);
    EXPECT_EQ(mem_ex->base.antigen_id, response.antigen_id);

    // Verify antigen source mapping is correct
    exception_antigen_source_t source = nimcp_exception_to_antigen_source(EXCEPTION_CATEGORY_MEMORY);
    EXPECT_EQ(source, EX_ANTIGEN_SOURCE_ANOMALY);  // Memory -> ANOMALY

    // Verify severity mapping
    uint32_t immune_severity = nimcp_exception_to_immune_severity(EXCEPTION_SEVERITY_SEVERE);
    EXPECT_EQ(immune_severity, 7u);  // SEVERE maps to 7

    nimcp_exception_unref((nimcp_exception_t*)mem_ex);
}

TEST_F(ExceptionImmuneIntegrationTest, MemoryExceptionRecoveryStrategy) {
    // WHAT: Test recovery strategy for memory exceptions
    // WHY:  Verify GC is primary recovery action for memory errors

    nimcp_exception_t* ex = createTestException(
        NIMCP_ERROR_NO_MEMORY,
        EXCEPTION_SEVERITY_SEVERE,
        "Out of memory"
    );
    ASSERT_NE(ex, nullptr);

    nimcp_exception_recovery_strategy_t strategy;
    nimcp_exception_get_recovery_strategy(ex, &strategy);

    EXPECT_EQ(strategy.primary_action, EXCEPTION_RECOVERY_GC);
    EXPECT_EQ(strategy.fallback_action, EXCEPTION_RECOVERY_QUARANTINE);
    EXPECT_EQ(strategy.retry_count, 3u);
    EXPECT_EQ(strategy.cooldown_ms, 100u);

    nimcp_exception_unref(ex);
}

/* ============================================================================
 * Brain Exception Presentation Tests
 * ============================================================================ */

TEST_F(ExceptionImmuneIntegrationTest, PresentBrainExceptionToImmune) {
    // WHAT: Test presenting a brain exception to the immune system
    // WHY:  Verify brain/neural errors trigger correct immune response

    nimcp_brain_exception_t* brain_ex = nimcp_brain_exception_create(
        NIMCP_ERROR_LEARNING_FAILED,
        EXCEPTION_SEVERITY_CRITICAL,
        __FILE__, __LINE__, __func__,
        1,  // brain_id
        "prefrontal",  // region_name
        "Learning diverged with gradient explosion"
    );
    ASSERT_NE(brain_ex, nullptr);

    // Set brain-specific fields
    brain_ex->network_id = 42;
    brain_ex->gradient_norm = 1e10f;  // Exploding gradient
    brain_ex->has_nan_weights = true;
    brain_ex->learning_diverged = true;

    // Verify category is BRAIN
    EXPECT_EQ(brain_ex->base.category, EXCEPTION_CATEGORY_BRAIN);

    // Present to immune system
    nimcp_immune_response_t response;
    memset(&response, 0, sizeof(response));
    int result = nimcp_exception_present_to_immune((nimcp_exception_t*)brain_ex, &response);
    EXPECT_EQ(result, 0);

    // Verify exception was marked as presented
    EXPECT_TRUE(brain_ex->base.presented_to_immune);
    EXPECT_GT(response.antigen_id, 0u);

    // Verify antigen source mapping
    exception_antigen_source_t source = nimcp_exception_to_antigen_source(EXCEPTION_CATEGORY_BRAIN);
    EXPECT_EQ(source, EX_ANTIGEN_SOURCE_BBB);  // Brain -> BBB

    // Verify severity mapping
    uint32_t immune_severity = nimcp_exception_to_immune_severity(EXCEPTION_SEVERITY_CRITICAL);
    EXPECT_EQ(immune_severity, 9u);  // CRITICAL maps to 9

    nimcp_exception_unref((nimcp_exception_t*)brain_ex);
}

TEST_F(ExceptionImmuneIntegrationTest, BrainExceptionRecoveryStrategy) {
    // WHAT: Test recovery strategy for brain exceptions
    // WHY:  Verify rollback is primary recovery action for brain errors

    nimcp_brain_exception_t* brain_ex = nimcp_brain_exception_create(
        NIMCP_ERROR_FORWARD_PASS,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        1, "hippocampus",
        "Forward pass failed"
    );
    ASSERT_NE(brain_ex, nullptr);

    nimcp_exception_recovery_strategy_t strategy;
    nimcp_exception_get_recovery_strategy((nimcp_exception_t*)brain_ex, &strategy);

    EXPECT_EQ(strategy.primary_action, EXCEPTION_RECOVERY_ROLLBACK);
    EXPECT_EQ(strategy.fallback_action, EXCEPTION_RECOVERY_REDUCE_LOAD);
    EXPECT_EQ(strategy.retry_count, 1u);
    EXPECT_EQ(strategy.cooldown_ms, 500u);

    nimcp_exception_unref((nimcp_exception_t*)brain_ex);
}

/* ============================================================================
 * Signal Exception Presentation Tests
 * ============================================================================ */

TEST_F(ExceptionImmuneIntegrationTest, PresentSignalExceptionToImmune) {
    // WHAT: Test presenting a signal exception to the immune system
    // WHY:  Verify signal/crash errors trigger emergency immune response

    nimcp_signal_exception_t* signal_ex = nimcp_signal_exception_create(
        SIGSEGV,
        (void*)0xDEADBEEF,  // fault_address
        __FILE__, __LINE__, __func__,
        "Segmentation fault at 0xDEADBEEF"
    );
    ASSERT_NE(signal_ex, nullptr);

    // Verify category is SIGNAL
    EXPECT_EQ(signal_ex->base.category, EXCEPTION_CATEGORY_SIGNAL);

    // Present to immune system
    nimcp_immune_response_t response;
    memset(&response, 0, sizeof(response));
    int result = nimcp_exception_present_to_immune((nimcp_exception_t*)signal_ex, &response);
    EXPECT_EQ(result, 0);

    // Verify exception was marked as presented
    EXPECT_TRUE(signal_ex->base.presented_to_immune);
    EXPECT_GT(response.antigen_id, 0u);

    // Verify antigen source mapping
    exception_antigen_source_t source = nimcp_exception_to_antigen_source(EXCEPTION_CATEGORY_SIGNAL);
    EXPECT_EQ(source, EX_ANTIGEN_SOURCE_ANOMALY);  // Signal -> ANOMALY

    // Verify severity mapping (SIGSEGV maps to FATAL)
    uint32_t immune_severity = nimcp_exception_to_immune_severity(EXCEPTION_SEVERITY_FATAL);
    EXPECT_EQ(immune_severity, 10u);  // FATAL maps to 10

    nimcp_exception_unref((nimcp_exception_t*)signal_ex);
}

TEST_F(ExceptionImmuneIntegrationTest, SignalExceptionRecoveryStrategy) {
    // WHAT: Test recovery strategy for signal exceptions
    // WHY:  Verify emergency save is primary action for crashes

    nimcp_signal_exception_t* signal_ex = nimcp_signal_exception_create(
        SIGFPE,
        nullptr,
        __FILE__, __LINE__, __func__,
        "Floating point exception"
    );
    ASSERT_NE(signal_ex, nullptr);

    nimcp_exception_recovery_strategy_t strategy;
    nimcp_exception_get_recovery_strategy((nimcp_exception_t*)signal_ex, &strategy);

    EXPECT_EQ(strategy.primary_action, EXCEPTION_RECOVERY_EMERGENCY_SAVE);
    EXPECT_EQ(strategy.fallback_action, EXCEPTION_RECOVERY_GRACEFUL_SHUTDOWN);
    EXPECT_EQ(strategy.retry_count, 1u);
    EXPECT_EQ(strategy.cooldown_ms, 0u);  // No cooldown for emergency

    nimcp_exception_unref((nimcp_exception_t*)signal_ex);
}

/* ============================================================================
 * Recovery Strategies for Different Exception Types
 * ============================================================================ */

TEST_F(ExceptionImmuneIntegrationTest, RecoveryStrategiesForDifferentExceptionTypes) {
    // WHAT: Test that each exception category has appropriate recovery strategy
    // WHY:  Verify recovery strategies match exception semantics

    struct TestCase {
        nimcp_error_t code;
        nimcp_exception_category_t expected_category;
        nimcp_exception_recovery_action_t expected_primary;
        nimcp_exception_recovery_action_t expected_fallback;
    };

    std::vector<TestCase> test_cases = {
        // Memory errors -> GC + Quarantine
        {NIMCP_ERROR_NO_MEMORY, EXCEPTION_CATEGORY_MEMORY,
         EXCEPTION_RECOVERY_GC, EXCEPTION_RECOVERY_QUARANTINE},

        // Brain errors -> Rollback + Reduce load
        {NIMCP_ERROR_BRAIN_CREATION, EXCEPTION_CATEGORY_BRAIN,
         EXCEPTION_RECOVERY_ROLLBACK, EXCEPTION_RECOVERY_REDUCE_LOAD},

        // I/O errors -> Retry + Rollback
        {NIMCP_ERROR_FILE_READ, EXCEPTION_CATEGORY_IO,
         EXCEPTION_RECOVERY_RETRY, EXCEPTION_RECOVERY_ROLLBACK},

        // Threading errors -> Restart thread + Graceful shutdown
        {NIMCP_ERROR_DEADLOCK, EXCEPTION_CATEGORY_THREADING,
         EXCEPTION_RECOVERY_RESTART_THREAD, EXCEPTION_RECOVERY_GRACEFUL_SHUTDOWN},

        // GPU errors -> Clear cache + Reduce load
        {NIMCP_ERROR_GPU, EXCEPTION_CATEGORY_GPU,
         EXCEPTION_RECOVERY_CLEAR_CACHE, EXCEPTION_RECOVERY_REDUCE_LOAD},

        // Cognitive errors -> GC + Reduce load
        {NIMCP_ERROR_WORKING_MEMORY, EXCEPTION_CATEGORY_COGNITIVE,
         EXCEPTION_RECOVERY_GC, EXCEPTION_RECOVERY_REDUCE_LOAD},
    };

    // Note: Security errors (9xxx range) are not tested here because the
    // nimcp_exception_get_category_from_code function doesn't have explicit
    // handling for the 9xxx range. Security exceptions should be created
    // using nimcp_security_exception_create which explicitly sets the category.

    for (const auto& tc : test_cases) {
        nimcp_exception_t* ex = nimcp_exception_create(
            tc.code, EXCEPTION_SEVERITY_ERROR,
            __FILE__, __LINE__, __func__,
            "Test exception for code %d", tc.code
        );
        ASSERT_NE(ex, nullptr) << "Failed to create exception for code " << tc.code;

        EXPECT_EQ(ex->category, tc.expected_category)
            << "Wrong category for error code " << tc.code;

        nimcp_exception_recovery_strategy_t strategy;
        nimcp_exception_get_recovery_strategy(ex, &strategy);

        EXPECT_EQ(strategy.primary_action, tc.expected_primary)
            << "Wrong primary action for error code " << tc.code;
        EXPECT_EQ(strategy.fallback_action, tc.expected_fallback)
            << "Wrong fallback action for error code " << tc.code;

        nimcp_exception_unref(ex);
    }
}

/* ============================================================================
 * Epitope Generation Tests
 * ============================================================================ */

TEST_F(ExceptionImmuneIntegrationTest, EpitopeGenerationConsistent) {
    // WHAT: Test that epitope generation is deterministic
    // WHY:  Same exception should produce same epitope for immune matching

    // Create two identical exceptions
    nimcp_exception_t* ex1 = nimcp_exception_create(
        NIMCP_ERROR_NO_MEMORY,
        EXCEPTION_SEVERITY_SEVERE,
        "test_file.c", 100, "test_func",
        "Consistent epitope test"
    );
    ASSERT_NE(ex1, nullptr);

    nimcp_exception_t* ex2 = nimcp_exception_create(
        NIMCP_ERROR_NO_MEMORY,
        EXCEPTION_SEVERITY_SEVERE,
        "test_file.c", 100, "test_func",
        "Consistent epitope test"
    );
    ASSERT_NE(ex2, nullptr);

    // Compute epitopes
    uint8_t epitope1[NIMCP_EXCEPTION_EPITOPE_SIZE];
    uint8_t epitope2[NIMCP_EXCEPTION_EPITOPE_SIZE];

    size_t len1 = nimcp_exception_compute_epitope(ex1, epitope1, sizeof(epitope1));
    size_t len2 = nimcp_exception_compute_epitope(ex2, epitope2, sizeof(epitope2));

    EXPECT_GT(len1, 0u);
    EXPECT_EQ(len1, len2);
    EXPECT_EQ(memcmp(epitope1, epitope2, len1), 0);

    nimcp_exception_unref(ex1);
    nimcp_exception_unref(ex2);
}

TEST_F(ExceptionImmuneIntegrationTest, EpitopeDifferentForDifferentExceptions) {
    // WHAT: Test that different exceptions produce different epitopes
    // WHY:  Immune system needs unique signatures for different threats

    nimcp_exception_t* ex1 = nimcp_exception_create(
        NIMCP_ERROR_NO_MEMORY,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        "Memory error"
    );
    ASSERT_NE(ex1, nullptr);

    nimcp_exception_t* ex2 = nimcp_exception_create(
        NIMCP_ERROR_BRAIN_CREATION,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        "Brain error"
    );
    ASSERT_NE(ex2, nullptr);

    uint8_t epitope1[NIMCP_EXCEPTION_EPITOPE_SIZE];
    uint8_t epitope2[NIMCP_EXCEPTION_EPITOPE_SIZE];

    size_t len1 = nimcp_exception_compute_epitope(ex1, epitope1, sizeof(epitope1));
    size_t len2 = nimcp_exception_compute_epitope(ex2, epitope2, sizeof(epitope2));

    EXPECT_GT(len1, 0u);
    EXPECT_GT(len2, 0u);

    // At minimum, the first 4 bytes (error code) should differ
    EXPECT_NE(memcmp(epitope1, epitope2, 4), 0);

    nimcp_exception_unref(ex1);
    nimcp_exception_unref(ex2);
}

TEST_F(ExceptionImmuneIntegrationTest, EpitopeContainsExpectedFields) {
    // WHAT: Test that epitope contains error code, category, severity, and type
    // WHY:  Verify epitope structure matches documentation

    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_NO_MEMORY,
        EXCEPTION_SEVERITY_CRITICAL,
        "test.c", 42, "test_func",
        "Test exception"
    );
    ASSERT_NE(ex, nullptr);

    uint8_t epitope[NIMCP_EXCEPTION_EPITOPE_SIZE];
    size_t len = nimcp_exception_compute_epitope(ex, epitope, sizeof(epitope));

    EXPECT_GE(len, 28u);  // At least 7 x 4-byte fields

    // Verify error code is at offset 0
    nimcp_error_t stored_code;
    memcpy(&stored_code, epitope, sizeof(stored_code));
    EXPECT_EQ(stored_code, NIMCP_ERROR_NO_MEMORY);

    // Verify category is at offset 4
    uint32_t stored_category;
    memcpy(&stored_category, epitope + 4, sizeof(stored_category));
    EXPECT_EQ(stored_category, (uint32_t)EXCEPTION_CATEGORY_MEMORY);

    // Verify severity is at offset 8
    uint32_t stored_severity;
    memcpy(&stored_severity, epitope + 8, sizeof(stored_severity));
    EXPECT_EQ(stored_severity, (uint32_t)EXCEPTION_SEVERITY_CRITICAL);

    nimcp_exception_unref(ex);
}

/* ============================================================================
 * Async Presentation Tests
 * ============================================================================ */

TEST_F(ExceptionImmuneIntegrationTest, AsyncPresentationProcessing) {
    // WHAT: Test async exception presentation and processing
    // WHY:  Verify non-blocking exception handling works correctly

    // Create multiple exceptions
    const int NUM_EXCEPTIONS = 5;
    std::vector<nimcp_exception_t*> exceptions;

    for (int i = 0; i < NUM_EXCEPTIONS; i++) {
        nimcp_exception_t* ex = nimcp_exception_create(
            NIMCP_ERROR_NO_MEMORY + i,
            EXCEPTION_SEVERITY_ERROR,
            __FILE__, __LINE__, __func__,
            "Async test exception %d", i
        );
        ASSERT_NE(ex, nullptr);
        exceptions.push_back(ex);
    }

    // Queue all exceptions for async presentation
    for (auto ex : exceptions) {
        int result = nimcp_exception_present_async(ex);
        EXPECT_EQ(result, 0);
    }

    // Get stats to verify pending count
    nimcp_exception_immune_stats_t stats;
    nimcp_exception_immune_get_stats(&stats);
    EXPECT_EQ(stats.exceptions_pending, (uint64_t)NUM_EXCEPTIONS);

    // Process all pending
    size_t processed = nimcp_exception_immune_process_pending(0);  // 0 = process all
    EXPECT_EQ(processed, (size_t)NUM_EXCEPTIONS);

    // Verify stats updated
    nimcp_exception_immune_get_stats(&stats);
    EXPECT_EQ(stats.exceptions_pending, 0u);
    EXPECT_GE(stats.exceptions_presented, (uint64_t)NUM_EXCEPTIONS);

    // Clean up
    for (auto ex : exceptions) {
        nimcp_exception_unref(ex);
    }
}

TEST_F(ExceptionImmuneIntegrationTest, AsyncPartialProcessing) {
    // WHAT: Test processing a limited number of async exceptions
    // WHY:  Verify max_count parameter works correctly

    const int NUM_EXCEPTIONS = 10;
    std::vector<nimcp_exception_t*> exceptions;

    for (int i = 0; i < NUM_EXCEPTIONS; i++) {
        nimcp_exception_t* ex = nimcp_exception_create(
            NIMCP_ERROR_NO_MEMORY,
            EXCEPTION_SEVERITY_WARNING,
            __FILE__, __LINE__, __func__,
            "Async partial test %d", i
        );
        ASSERT_NE(ex, nullptr);
        exceptions.push_back(ex);
        nimcp_exception_present_async(ex);
    }

    // Process only 3
    size_t processed = nimcp_exception_immune_process_pending(3);
    EXPECT_EQ(processed, 3u);

    nimcp_exception_immune_stats_t stats;
    nimcp_exception_immune_get_stats(&stats);
    EXPECT_EQ(stats.exceptions_pending, (uint64_t)(NUM_EXCEPTIONS - 3));

    // Process remaining
    processed = nimcp_exception_immune_process_pending(0);
    EXPECT_EQ(processed, (size_t)(NUM_EXCEPTIONS - 3));

    nimcp_exception_immune_get_stats(&stats);
    EXPECT_EQ(stats.exceptions_pending, 0u);

    for (auto ex : exceptions) {
        nimcp_exception_unref(ex);
    }
}

/* ============================================================================
 * Statistics Tracking Tests
 * ============================================================================ */

TEST_F(ExceptionImmuneIntegrationTest, StatisticsTracking) {
    // WHAT: Test that statistics are correctly tracked
    // WHY:  Verify monitoring data is accurate

    // Reset stats
    nimcp_exception_immune_reset_stats();

    nimcp_exception_immune_stats_t stats;
    nimcp_exception_immune_get_stats(&stats);
    EXPECT_EQ(stats.exceptions_presented, 0u);
    EXPECT_EQ(stats.recoveries_attempted, 0u);

    // Present several exceptions
    const int NUM_EXCEPTIONS = 5;
    for (int i = 0; i < NUM_EXCEPTIONS; i++) {
        nimcp_exception_t* ex = nimcp_exception_create(
            NIMCP_ERROR_NO_MEMORY,
            EXCEPTION_SEVERITY_SEVERE,
            __FILE__, __LINE__, __func__,
            "Stats test %d", i
        );
        ASSERT_NE(ex, nullptr);

        nimcp_immune_response_t response;
        nimcp_exception_present_to_immune(ex, &response);

        nimcp_exception_unref(ex);
    }

    // Verify stats
    nimcp_exception_immune_get_stats(&stats);
    EXPECT_EQ(stats.exceptions_presented, (uint64_t)NUM_EXCEPTIONS);
    // avg_response_time_us may be 0 if operations are very fast
    EXPECT_GE(stats.avg_response_time_us, 0.0f);
}

TEST_F(ExceptionImmuneIntegrationTest, QueueOverflowTracking) {
    // WHAT: Test that queue overflows are tracked
    // WHY:  Verify we can detect when the system is overwhelmed

    nimcp_exception_immune_reset_stats();

    // Fill the queue beyond capacity
    const size_t OVERFLOW_COUNT = NIMCP_EXCEPTION_IMMUNE_QUEUE_SIZE + 10;
    std::vector<nimcp_exception_t*> exceptions;

    for (size_t i = 0; i < OVERFLOW_COUNT; i++) {
        nimcp_exception_t* ex = nimcp_exception_create(
            NIMCP_ERROR_NO_MEMORY,
            EXCEPTION_SEVERITY_WARNING,
            __FILE__, __LINE__, __func__,
            "Overflow test %zu", i
        );
        ASSERT_NE(ex, nullptr);
        exceptions.push_back(ex);

        int result = nimcp_exception_present_async(ex);
        // After queue is full, result should be -1
        if (i >= NIMCP_EXCEPTION_IMMUNE_QUEUE_SIZE) {
            EXPECT_EQ(result, -1);
        }
    }

    // Verify overflow count
    nimcp_exception_immune_stats_t stats;
    nimcp_exception_immune_get_stats(&stats);
    EXPECT_GE(stats.queue_overflows, 10u);

    // Process all to clean up
    nimcp_exception_immune_process_pending(0);

    for (auto ex : exceptions) {
        nimcp_exception_unref(ex);
    }
}

/* ============================================================================
 * Antigen Source Mapping Tests
 * ============================================================================ */

TEST_F(ExceptionImmuneIntegrationTest, AntigenSourceMapping) {
    // WHAT: Test that exception categories map to correct antigen sources
    // WHY:  Verify immune system receives correct threat classification

    struct TestCase {
        nimcp_exception_category_t category;
        exception_antigen_source_t expected_source;
    };

    std::vector<TestCase> test_cases = {
        {EXCEPTION_CATEGORY_SECURITY, EX_ANTIGEN_SOURCE_BBB},
        {EXCEPTION_CATEGORY_BRAIN, EX_ANTIGEN_SOURCE_BBB},
        {EXCEPTION_CATEGORY_BRAIN_REGION, EX_ANTIGEN_SOURCE_BBB},
        {EXCEPTION_CATEGORY_THREADING, EX_ANTIGEN_SOURCE_BFT},
        {EXCEPTION_CATEGORY_MEMORY, EX_ANTIGEN_SOURCE_ANOMALY},
        {EXCEPTION_CATEGORY_IO, EX_ANTIGEN_SOURCE_ANOMALY},
        {EXCEPTION_CATEGORY_SIGNAL, EX_ANTIGEN_SOURCE_ANOMALY},
        {EXCEPTION_CATEGORY_COGNITIVE, EX_ANTIGEN_SOURCE_ANOMALY},
        {EXCEPTION_CATEGORY_GPU, EX_ANTIGEN_SOURCE_ANOMALY},
    };

    for (const auto& tc : test_cases) {
        exception_antigen_source_t source = nimcp_exception_to_antigen_source(tc.category);
        EXPECT_EQ(source, tc.expected_source)
            << "Wrong source for category " << (int)tc.category;
    }
}

TEST_F(ExceptionImmuneIntegrationTest, SeverityMapping) {
    // WHAT: Test that exception severities map to correct immune severities
    // WHY:  Verify immune system receives correct threat severity

    struct TestCase {
        nimcp_exception_severity_t exception_severity;
        uint32_t expected_immune_severity;
    };

    std::vector<TestCase> test_cases = {
        {EXCEPTION_SEVERITY_DEBUG, 1},
        {EXCEPTION_SEVERITY_INFO, 2},
        {EXCEPTION_SEVERITY_WARNING, 3},
        {EXCEPTION_SEVERITY_ERROR, 5},
        {EXCEPTION_SEVERITY_SEVERE, 7},
        {EXCEPTION_SEVERITY_CRITICAL, 9},
        {EXCEPTION_SEVERITY_FATAL, 10},
    };

    for (const auto& tc : test_cases) {
        uint32_t immune_severity = nimcp_exception_to_immune_severity(tc.exception_severity);
        EXPECT_EQ(immune_severity, tc.expected_immune_severity)
            << "Wrong immune severity for exception severity "
            << (int)tc.exception_severity;
    }
}

/* ============================================================================
 * Connection State Tests
 * ============================================================================ */

TEST_F(ExceptionImmuneIntegrationTest, ConnectionState) {
    // WHAT: Test connection state tracking
    // WHY:  Verify connection state is correctly reported

    EXPECT_TRUE(nimcp_exception_immune_is_connected());

    nimcp_exception_immune_disconnect();
    EXPECT_FALSE(nimcp_exception_immune_is_connected());

    // Reconnect
    int result = nimcp_exception_immune_connect(immune_system);
    EXPECT_EQ(result, 0);
    EXPECT_TRUE(nimcp_exception_immune_is_connected());
}

TEST_F(ExceptionImmuneIntegrationTest, PresentWithoutConnection) {
    // WHAT: Test presenting exception when not connected
    // WHY:  Verify graceful handling when immune system unavailable

    nimcp_exception_immune_disconnect();

    nimcp_exception_t* ex = createTestException(
        NIMCP_ERROR_NO_MEMORY,
        EXCEPTION_SEVERITY_ERROR,
        "Test without connection"
    );
    ASSERT_NE(ex, nullptr);

    // Should still succeed (marks as presented, uses fallback ID)
    nimcp_immune_response_t response;
    memset(&response, 0, sizeof(response));
    int result = nimcp_exception_present_to_immune(ex, &response);
    EXPECT_EQ(result, 0);

    // Exception should be marked as presented
    EXPECT_TRUE(ex->presented_to_immune);
    EXPECT_GT(ex->antigen_id, 0u);

    // Response should indicate no recovery (no immune system)
    EXPECT_FALSE(response.recovery_attempted);

    nimcp_exception_unref(ex);

    // Reconnect for cleanup
    nimcp_exception_immune_connect(immune_system);
}

/* ============================================================================
 * Re-presentation Prevention Tests
 * ============================================================================ */

TEST_F(ExceptionImmuneIntegrationTest, PreventDuplicatePresentation) {
    // WHAT: Test that exceptions are not presented twice
    // WHY:  Prevent duplicate antigen creation

    nimcp_exception_t* ex = createTestException(
        NIMCP_ERROR_NO_MEMORY,
        EXCEPTION_SEVERITY_ERROR,
        "No duplicate test"
    );
    ASSERT_NE(ex, nullptr);

    // First presentation
    nimcp_immune_response_t response1;
    int result1 = nimcp_exception_present_to_immune(ex, &response1);
    EXPECT_EQ(result1, 0);
    uint32_t first_antigen_id = response1.antigen_id;

    // Second presentation should be no-op
    nimcp_immune_response_t response2;
    memset(&response2, 0, sizeof(response2));
    int result2 = nimcp_exception_present_to_immune(ex, &response2);
    EXPECT_EQ(result2, 0);

    // Antigen ID should remain the same
    EXPECT_EQ(ex->antigen_id, first_antigen_id);

    nimcp_exception_unref(ex);
}

/* ============================================================================
 * Threading Exception Tests
 * ============================================================================ */

TEST_F(ExceptionImmuneIntegrationTest, ThreadingExceptionPresentation) {
    // WHAT: Test presenting threading exceptions
    // WHY:  Verify threading errors map to BFT source

    nimcp_threading_exception_t* thread_ex = nimcp_threading_exception_create(
        NIMCP_ERROR_DEADLOCK,
        EXCEPTION_SEVERITY_CRITICAL,
        __FILE__, __LINE__, __func__,
        12345,  // thread_id
        "Deadlock detected between threads"
    );
    ASSERT_NE(thread_ex, nullptr);

    thread_ex->is_deadlock = true;
    thread_ex->deadlock_cycle_len = 3;

    // Verify category is THREADING
    EXPECT_EQ(thread_ex->base.category, EXCEPTION_CATEGORY_THREADING);

    nimcp_immune_response_t response;
    memset(&response, 0, sizeof(response));
    int result = nimcp_exception_present_to_immune((nimcp_exception_t*)thread_ex, &response);
    EXPECT_EQ(result, 0);

    // Verify exception was marked as presented
    EXPECT_TRUE(thread_ex->base.presented_to_immune);
    EXPECT_GT(response.antigen_id, 0u);

    // Verify antigen source mapping
    exception_antigen_source_t source = nimcp_exception_to_antigen_source(EXCEPTION_CATEGORY_THREADING);
    EXPECT_EQ(source, EX_ANTIGEN_SOURCE_BFT);  // Threading -> BFT

    nimcp_exception_recovery_strategy_t strategy;
    nimcp_exception_get_recovery_strategy((nimcp_exception_t*)thread_ex, &strategy);
    EXPECT_EQ(strategy.primary_action, EXCEPTION_RECOVERY_RESTART_THREAD);

    nimcp_exception_unref((nimcp_exception_t*)thread_ex);
}

/* ============================================================================
 * Security Exception Tests
 * ============================================================================ */

TEST_F(ExceptionImmuneIntegrationTest, SecurityExceptionPresentation) {
    // WHAT: Test presenting security exceptions
    // WHY:  Verify security threats map to BBB source

    nimcp_security_exception_t* sec_ex = nimcp_security_exception_create(
        NIMCP_ERROR_BBB_REJECTED,
        EXCEPTION_SEVERITY_CRITICAL,
        __FILE__, __LINE__, __func__,
        1,  // threat_type (BBB_THREAT_SQL_INJECTION)
        "SQL injection attempt detected"
    );
    ASSERT_NE(sec_ex, nullptr);

    sec_ex->quarantine_required = true;
    sec_ex->severity_score = 9;

    // Verify category is SECURITY (explicitly set by nimcp_security_exception_create)
    EXPECT_EQ(sec_ex->base.category, EXCEPTION_CATEGORY_SECURITY);

    nimcp_immune_response_t response;
    memset(&response, 0, sizeof(response));
    int result = nimcp_exception_present_to_immune((nimcp_exception_t*)sec_ex, &response);
    EXPECT_EQ(result, 0);

    // Verify exception was marked as presented
    EXPECT_TRUE(sec_ex->base.presented_to_immune);
    EXPECT_GT(response.antigen_id, 0u);

    // Verify antigen source mapping
    exception_antigen_source_t source = nimcp_exception_to_antigen_source(EXCEPTION_CATEGORY_SECURITY);
    EXPECT_EQ(source, EX_ANTIGEN_SOURCE_BBB);  // Security -> BBB

    nimcp_exception_recovery_strategy_t strategy;
    nimcp_exception_get_recovery_strategy((nimcp_exception_t*)sec_ex, &strategy);
    EXPECT_EQ(strategy.primary_action, EXCEPTION_RECOVERY_QUARANTINE);

    nimcp_exception_unref((nimcp_exception_t*)sec_ex);
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
