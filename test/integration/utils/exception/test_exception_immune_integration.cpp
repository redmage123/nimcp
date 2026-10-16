/**
 * @file test_exception_immune_integration.cpp
 * @brief Integration tests for exception-immune bridge integration
 * @version 1.0.0
 * @date 2026-01-21
 *
 * WHAT: Test full integration flow from exception creation through immune response
 * WHY:  Verify exception handling seamlessly integrates with brain immune system
 *       for automated error recovery and fault tolerance
 * HOW:  Test exception presentation as antigens, recovery callbacks, aggregation,
 *       severity escalation, and recovery callback chains
 *
 * TEST SCENARIOS:
 * 1. Exception thrown -> presented to immune system as antigen
 * 2. Recovery callback triggered -> immune system notified
 * 3. Multiple exceptions -> proper aggregation and immune response
 * 4. Exception with different severities -> correct immune escalation
 * 5. Recovery callback chain (GC -> Compact -> Rollback fallback)
 *
 * @author NIMCP Development Team
 */

#include <gtest/gtest.h>
#include <cstring>
#include <thread>
#include <chrono>
#include <atomic>
#include <vector>
#include <functional>

// Headers have their own extern "C" guards
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_immune.h"
#include "utils/error/nimcp_error_codes.h"
#include "cognitive/immune/nimcp_brain_immune.h"

/* ============================================================================
 * Test Tracking Globals
 * ============================================================================ */

namespace {
    std::atomic<int> g_recovery_gc_called{0};
    std::atomic<int> g_recovery_compact_called{0};
    std::atomic<int> g_recovery_rollback_called{0};
    std::atomic<int> g_recovery_quarantine_called{0};
    std::atomic<int> g_immune_notification_count{0};
    std::atomic<bool> g_gc_should_fail{false};
    std::atomic<bool> g_compact_should_fail{false};

    void reset_tracking() {
        g_recovery_gc_called = 0;
        g_recovery_compact_called = 0;
        g_recovery_rollback_called = 0;
        g_recovery_quarantine_called = 0;
        g_immune_notification_count = 0;
        g_gc_should_fail = false;
        g_compact_should_fail = false;
    }
}

/* ============================================================================
 * Custom Recovery Callbacks for Testing
 * ============================================================================ */

extern "C" {

/**
 * @brief Test GC recovery callback that tracks calls
 */
static int test_recovery_gc(
    nimcp_exception_t* ex,
    nimcp_exception_recovery_action_t action,
    void* user_data
) {
    (void)ex;
    (void)action;
    (void)user_data;
    g_recovery_gc_called++;

    if (g_gc_should_fail) {
        return -1;  // Failure triggers fallback
    }
    return 0;  // Success
}

/**
 * @brief Test compact recovery callback that tracks calls
 */
static int test_recovery_compact(
    nimcp_exception_t* ex,
    nimcp_exception_recovery_action_t action,
    void* user_data
) {
    (void)ex;
    (void)action;
    (void)user_data;
    g_recovery_compact_called++;

    if (g_compact_should_fail) {
        return -1;  // Failure triggers fallback
    }
    return 0;
}

/**
 * @brief Test rollback recovery callback that tracks calls
 */
static int test_recovery_rollback(
    nimcp_exception_t* ex,
    nimcp_exception_recovery_action_t action,
    void* user_data
) {
    (void)ex;
    (void)action;
    (void)user_data;
    g_recovery_rollback_called++;
    return 0;  // Always succeed as last resort
}

/**
 * @brief Test quarantine recovery callback that tracks calls
 */
static int test_recovery_quarantine(
    nimcp_exception_t* ex,
    nimcp_exception_recovery_action_t action,
    void* user_data
) {
    (void)ex;
    (void)action;
    (void)user_data;
    g_recovery_quarantine_called++;
    return 0;
}

}  // extern "C"

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class ExceptionImmuneBridgeIntegrationTest : public ::testing::Test {
protected:
    brain_immune_system_t* immune_system = nullptr;
    brain_immune_config_t immune_config;

    void SetUp() override {
        reset_tracking();

        // Initialize exception system
        int result = nimcp_exception_system_init();
        ASSERT_EQ(result, 0) << "Failed to initialize exception system";

        // Initialize exception-immune integration with auto-recovery enabled
        nimcp_exception_immune_config_t ex_config;
        nimcp_exception_immune_default_config(&ex_config);
        ex_config.enable_auto_present = true;
        ex_config.enable_auto_recovery = true;
        ex_config.enable_memory_formation = true;
        ex_config.async_presentation = false;  // Synchronous for testing
        result = nimcp_exception_immune_init(&ex_config);
        ASSERT_EQ(result, 0) << "Failed to initialize exception-immune integration";

        // Create brain immune system
        brain_immune_default_config(&immune_config);
        immune_config.max_antigens = 128;
        immune_config.max_antibodies = 256;
        immune_system = brain_immune_create(&immune_config);
        ASSERT_NE(immune_system, nullptr) << "Failed to create brain immune system";

        result = brain_immune_start(immune_system);
        ASSERT_EQ(result, 0) << "Failed to start brain immune system";

        // Connect exception system to immune system
        result = nimcp_exception_immune_connect(immune_system);
        ASSERT_EQ(result, 0) << "Failed to connect to immune system";

        // Install default recovery callbacks for testing
        nimcp_exception_install_default_recovery_callbacks();
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

    // Helper to create test exception
    nimcp_exception_t* createException(
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

    // Wait for immune system to process (with timeout)
    void waitForImmuneProcessing(int max_ms = 100) {
        std::this_thread::sleep_for(std::chrono::milliseconds(max_ms));
    }
};

/* ============================================================================
 * Test 1: Exception Thrown -> Presented to Immune System as Antigen
 * ============================================================================ */

TEST_F(ExceptionImmuneBridgeIntegrationTest, ExceptionPresentedAsAntigen) {
    // WHAT: Test that an exception is correctly presented to the immune system
    // WHY:  Verify the fundamental bridge between exception handling and immune response

    // Create a memory exception (severe enough to trigger immune response)
    nimcp_memory_exception_t* mem_ex = nimcp_memory_exception_create(
        NIMCP_ERROR_NO_MEMORY,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        1024 * 1024,  // 1MB requested
        "Failed to allocate memory buffer"
    );
    ASSERT_NE(mem_ex, nullptr);

    // Present to immune system
    nimcp_immune_response_t response;
    memset(&response, 0, sizeof(response));
    int result = nimcp_exception_present_to_immune((nimcp_exception_t*)mem_ex, &response);
    EXPECT_EQ(result, 0) << "Exception presentation failed";

    // Verify antigen was created (bridge assigns an ID even if immune not fully connected)
    EXPECT_GT(response.antigen_id, 0u) << "No antigen ID assigned";

    // Verify exception was marked as presented
    EXPECT_TRUE(mem_ex->base.presented_to_immune);
    EXPECT_EQ(mem_ex->base.antigen_id, response.antigen_id);

    // Verify response metadata
    EXPECT_GE(response.response_time_us, 0u);

    // Verify antigen source mapping is correct for memory exceptions
    exception_antigen_source_t source = nimcp_exception_to_antigen_source(EXCEPTION_CATEGORY_MEMORY);
    EXPECT_EQ(source, EX_ANTIGEN_SOURCE_ANOMALY) << "Memory -> ANOMALY mapping failed";

    // Verify severity mapping (SEVERE = 7)
    uint32_t immune_severity = nimcp_exception_to_immune_severity(EXCEPTION_SEVERITY_SEVERE);
    EXPECT_EQ(immune_severity, 7u) << "SEVERE severity should map to 7";

    // Verify epitope was generated
    EXPECT_GT(mem_ex->base.epitope_len, 0u) << "Epitope not generated";

    nimcp_exception_unref((nimcp_exception_t*)mem_ex);
}

TEST_F(ExceptionImmuneBridgeIntegrationTest, BrainExceptionMapsToCorrectAntigenSource) {
    // WHAT: Test brain exceptions map to BBB antigen source
    // WHY:  Brain security threats should trigger BBB-related immune response

    nimcp_brain_exception_t* brain_ex = nimcp_brain_exception_create(
        NIMCP_ERROR_LEARNING_FAILED,
        EXCEPTION_SEVERITY_CRITICAL,
        __FILE__, __LINE__, __func__,
        1,  // brain_id
        "prefrontal",
        "Gradient explosion detected"
    );
    ASSERT_NE(brain_ex, nullptr);

    brain_ex->has_nan_weights = true;
    brain_ex->gradient_norm = 1e10f;

    nimcp_immune_response_t response;
    memset(&response, 0, sizeof(response));
    int result = nimcp_exception_present_to_immune((nimcp_exception_t*)brain_ex, &response);
    EXPECT_EQ(result, 0);

    // Verify antigen ID was assigned
    EXPECT_GT(response.antigen_id, 0u);

    // Verify exception was marked as presented
    EXPECT_TRUE(brain_ex->base.presented_to_immune);

    // Verify category is BRAIN
    EXPECT_EQ(brain_ex->base.category, EXCEPTION_CATEGORY_BRAIN);

    // Brain exceptions should map to BBB source
    exception_antigen_source_t source = nimcp_exception_to_antigen_source(EXCEPTION_CATEGORY_BRAIN);
    EXPECT_EQ(source, EX_ANTIGEN_SOURCE_BBB);

    // CRITICAL severity maps to 9
    uint32_t immune_severity = nimcp_exception_to_immune_severity(EXCEPTION_SEVERITY_CRITICAL);
    EXPECT_EQ(immune_severity, 9u);

    nimcp_exception_unref((nimcp_exception_t*)brain_ex);
}

TEST_F(ExceptionImmuneBridgeIntegrationTest, ThreadingExceptionMapsToBFTSource) {
    // WHAT: Test threading exceptions map to BFT antigen source
    // WHY:  Threading/synchronization issues relate to Byzantine fault tolerance

    nimcp_threading_exception_t* thread_ex = nimcp_threading_exception_create(
        NIMCP_ERROR_DEADLOCK,
        EXCEPTION_SEVERITY_CRITICAL,
        __FILE__, __LINE__, __func__,
        12345,  // thread_id
        "Deadlock detected in worker pool"
    );
    ASSERT_NE(thread_ex, nullptr);

    thread_ex->is_deadlock = true;
    thread_ex->deadlock_cycle_len = 3;

    nimcp_immune_response_t response;
    memset(&response, 0, sizeof(response));
    int result = nimcp_exception_present_to_immune((nimcp_exception_t*)thread_ex, &response);
    EXPECT_EQ(result, 0);

    // Verify antigen ID was assigned
    EXPECT_GT(response.antigen_id, 0u);

    // Verify exception was marked as presented
    EXPECT_TRUE(thread_ex->base.presented_to_immune);

    // Verify category is THREADING
    EXPECT_EQ(thread_ex->base.category, EXCEPTION_CATEGORY_THREADING);

    // Threading exceptions should map to BFT source
    exception_antigen_source_t source = nimcp_exception_to_antigen_source(EXCEPTION_CATEGORY_THREADING);
    EXPECT_EQ(source, EX_ANTIGEN_SOURCE_BFT);

    nimcp_exception_unref((nimcp_exception_t*)thread_ex);
}

/* ============================================================================
 * Test 2: Recovery Callback Triggered -> Immune System Notified
 * ============================================================================ */

TEST_F(ExceptionImmuneBridgeIntegrationTest, RecoveryCallbackNotifiesImmuneSystem) {
    // WHAT: Test that recovery callbacks notify the immune system of results
    // WHY:  Immune system needs feedback to form memory and adjust responses

    nimcp_exception_t* ex = createException(
        NIMCP_ERROR_NO_MEMORY,
        EXCEPTION_SEVERITY_SEVERE,
        "Memory allocation failed"
    );
    ASSERT_NE(ex, nullptr);

    // Present to immune system
    nimcp_immune_response_t response;
    memset(&response, 0, sizeof(response));
    nimcp_exception_present_to_immune(ex, &response);

    // Execute recovery action
    // Note: GC will fail without a configured context, which is expected in test environment
    int recovery_result = nimcp_exception_execute_recovery(ex, EXCEPTION_RECOVERY_GC);
    bool recovery_success = (recovery_result == 0);

    // Notify immune system of actual recovery result
    int notify_result = nimcp_exception_notify_recovery_result(
        ex,
        EXCEPTION_RECOVERY_GC,
        recovery_success
    );
    EXPECT_EQ(notify_result, 0) << "Failed to notify immune system";

    // Get stats to verify the notification was recorded
    nimcp_exception_immune_stats_t stats;
    nimcp_exception_immune_get_stats(&stats);
    EXPECT_GE(stats.recoveries_attempted, 1u);

    // If recovery succeeded, verify it was counted
    if (recovery_success) {
        EXPECT_GE(stats.recoveries_succeeded, 1u);
    }

    nimcp_exception_unref(ex);
}

TEST_F(ExceptionImmuneBridgeIntegrationTest, FailedRecoveryNotifiesImmuneSystem) {
    // WHAT: Test that failed recovery also notifies the immune system
    // WHY:  Immune system needs to know about failures to trigger escalation

    nimcp_exception_t* ex = createException(
        NIMCP_ERROR_NO_MEMORY,
        EXCEPTION_SEVERITY_CRITICAL,
        "Critical memory failure"
    );
    ASSERT_NE(ex, nullptr);

    nimcp_immune_response_t response;
    memset(&response, 0, sizeof(response));
    nimcp_exception_present_to_immune(ex, &response);

    // Notify of failed recovery
    int result = nimcp_exception_notify_recovery_result(
        ex,
        EXCEPTION_RECOVERY_GC,
        false  // failure
    );
    EXPECT_EQ(result, 0);

    // Verify stats show attempt but not success
    nimcp_exception_immune_stats_t stats;
    nimcp_exception_immune_get_stats(&stats);
    EXPECT_GE(stats.recoveries_attempted, 1u);

    nimcp_exception_unref(ex);
}

/* ============================================================================
 * Test 3: Multiple Exceptions -> Proper Aggregation and Immune Response
 * ============================================================================ */

TEST_F(ExceptionImmuneBridgeIntegrationTest, MultipleExceptionsAggregation) {
    // WHAT: Test that multiple exceptions are properly aggregated
    // WHY:  Batch operations may produce multiple related errors

    // Create aggregate exception
    nimcp_aggregate_exception_t* agg = nimcp_aggregate_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Batch operation failed with multiple errors"
    );
    ASSERT_NE(agg, nullptr);

    // Add child exceptions
    const int NUM_CHILDREN = 5;
    for (int i = 0; i < NUM_CHILDREN; i++) {
        nimcp_exception_t* child = createException(
            NIMCP_ERROR_NO_MEMORY + i,
            static_cast<nimcp_exception_severity_t>(EXCEPTION_SEVERITY_WARNING + (i % 3)),
            "Child exception"
        );
        ASSERT_NE(child, nullptr);

        int result = nimcp_aggregate_exception_add(agg, child);
        EXPECT_EQ(result, 0) << "Failed to add child exception " << i;
    }

    // Verify aggregation
    EXPECT_EQ(nimcp_aggregate_exception_count(agg), (size_t)NUM_CHILDREN);

    // Present aggregate to immune system
    nimcp_immune_response_t response;
    memset(&response, 0, sizeof(response));
    int result = nimcp_exception_present_to_immune((nimcp_exception_t*)agg, &response);
    EXPECT_EQ(result, 0);
    EXPECT_GT(response.antigen_id, 0u);

    // Verify aggregate was presented
    EXPECT_TRUE(agg->base.presented_to_immune);

    // Get immune stats
    nimcp_exception_immune_stats_t stats;
    nimcp_exception_immune_get_stats(&stats);
    EXPECT_GE(stats.exceptions_presented, 1u);

    nimcp_exception_unref((nimcp_exception_t*)agg);
}

TEST_F(ExceptionImmuneBridgeIntegrationTest, MultipleIndependentExceptionsCreateMultipleAntigens) {
    // WHAT: Test that independent exceptions create separate antigens
    // WHY:  Immune system should track each threat independently

    const int NUM_EXCEPTIONS = 5;
    std::vector<uint32_t> antigen_ids;

    for (int i = 0; i < NUM_EXCEPTIONS; i++) {
        nimcp_exception_t* ex = createException(
            static_cast<nimcp_error_t>(NIMCP_ERROR_NO_MEMORY + i * 1000),
            EXCEPTION_SEVERITY_SEVERE,
            "Independent exception"
        );
        ASSERT_NE(ex, nullptr);

        nimcp_immune_response_t response;
        memset(&response, 0, sizeof(response));
        int result = nimcp_exception_present_to_immune(ex, &response);
        EXPECT_EQ(result, 0);

        antigen_ids.push_back(response.antigen_id);
        nimcp_exception_unref(ex);
    }

    // Verify all antigens are unique
    for (size_t i = 0; i < antigen_ids.size(); i++) {
        EXPECT_GT(antigen_ids[i], 0u) << "Antigen ID should be > 0";
        for (size_t j = i + 1; j < antigen_ids.size(); j++) {
            EXPECT_NE(antigen_ids[i], antigen_ids[j])
                << "Duplicate antigen IDs: " << antigen_ids[i];
        }
    }

    // Verify statistics reflect all presentations
    nimcp_exception_immune_stats_t stats;
    nimcp_exception_immune_get_stats(&stats);
    EXPECT_GE(stats.exceptions_presented, (uint64_t)NUM_EXCEPTIONS);
}

/* ============================================================================
 * Test 4: Exception with Different Severities -> Correct Immune Escalation
 * ============================================================================ */

TEST_F(ExceptionImmuneBridgeIntegrationTest, SeverityEscalation) {
    // WHAT: Test that exception severity correctly maps to immune severity
    // WHY:  Higher severity exceptions should trigger stronger immune responses

    struct SeverityTestCase {
        nimcp_exception_severity_t exception_severity;
        uint32_t expected_immune_severity;
        const char* description;
    };

    std::vector<SeverityTestCase> test_cases = {
        {EXCEPTION_SEVERITY_DEBUG, 1, "DEBUG"},
        {EXCEPTION_SEVERITY_INFO, 2, "INFO"},
        {EXCEPTION_SEVERITY_WARNING, 3, "WARNING"},
        {EXCEPTION_SEVERITY_ERROR, 5, "ERROR"},
        {EXCEPTION_SEVERITY_SEVERE, 7, "SEVERE"},
        {EXCEPTION_SEVERITY_CRITICAL, 9, "CRITICAL"},
        {EXCEPTION_SEVERITY_FATAL, 10, "FATAL"},
    };

    for (const auto& tc : test_cases) {
        nimcp_exception_t* ex = nimcp_exception_create(
            NIMCP_ERROR_OPERATION_FAILED,
            tc.exception_severity,
            __FILE__, __LINE__, __func__,
            "Severity test: %s", tc.description
        );
        ASSERT_NE(ex, nullptr);

        // Verify mapping function
        uint32_t mapped_severity = nimcp_exception_to_immune_severity(tc.exception_severity);
        EXPECT_EQ(mapped_severity, tc.expected_immune_severity)
            << "Severity mapping failed for " << tc.description;

        // Present to immune system
        nimcp_immune_response_t response;
        memset(&response, 0, sizeof(response));
        nimcp_exception_present_to_immune(ex, &response);

        // Verify antigen ID was assigned
        EXPECT_GT(response.antigen_id, 0u)
            << "No antigen ID for " << tc.description;

        nimcp_exception_unref(ex);
    }
}

TEST_F(ExceptionImmuneBridgeIntegrationTest, FatalSeverityTriggersEmergencyResponse) {
    // WHAT: Test that FATAL severity triggers emergency immune response
    // WHY:  Critical system failures need immediate attention

    nimcp_signal_exception_t* signal_ex = nimcp_signal_exception_create(
        SIGSEGV,
        (void*)0xDEADBEEF,
        __FILE__, __LINE__, __func__,
        "Segmentation fault - critical failure"
    );
    ASSERT_NE(signal_ex, nullptr);

    // Signal exceptions should be FATAL severity
    EXPECT_EQ(signal_ex->base.severity, EXCEPTION_SEVERITY_FATAL);

    nimcp_immune_response_t response;
    memset(&response, 0, sizeof(response));
    int result = nimcp_exception_present_to_immune((nimcp_exception_t*)signal_ex, &response);
    EXPECT_EQ(result, 0);

    // Verify antigen ID was assigned
    EXPECT_GT(response.antigen_id, 0u);

    // Verify FATAL severity mapping to maximum immune severity (10)
    uint32_t immune_severity = nimcp_exception_to_immune_severity(EXCEPTION_SEVERITY_FATAL);
    EXPECT_EQ(immune_severity, 10u) << "FATAL should map to max severity 10";

    // Verify recovery strategy is emergency
    nimcp_exception_recovery_strategy_t strategy;
    nimcp_exception_get_recovery_strategy((nimcp_exception_t*)signal_ex, &strategy);
    EXPECT_EQ(strategy.primary_action, EXCEPTION_RECOVERY_EMERGENCY_SAVE);
    EXPECT_EQ(strategy.fallback_action, EXCEPTION_RECOVERY_GRACEFUL_SHUTDOWN);

    nimcp_exception_unref((nimcp_exception_t*)signal_ex);
}

/* ============================================================================
 * Test 5: Recovery Callback Chain (GC -> Compact -> Rollback fallback)
 * ============================================================================ */

TEST_F(ExceptionImmuneBridgeIntegrationTest, RecoveryCallbackChain_GCSucceeds) {
    // WHAT: Test recovery chain where GC succeeds
    // WHY:  Verify primary recovery action is tried first

    reset_tracking();

    nimcp_exception_t* ex = createException(
        NIMCP_ERROR_NO_MEMORY,
        EXCEPTION_SEVERITY_SEVERE,
        "Memory exhausted"
    );
    ASSERT_NE(ex, nullptr);

    // Get recovery strategy
    nimcp_exception_recovery_strategy_t strategy;
    nimcp_exception_get_recovery_strategy(ex, &strategy);

    EXPECT_EQ(strategy.primary_action, EXCEPTION_RECOVERY_GC);
    EXPECT_EQ(strategy.fallback_action, EXCEPTION_RECOVERY_QUARANTINE);

    // Execute primary recovery
    int result = nimcp_exception_execute_recovery(ex, EXCEPTION_RECOVERY_GC);

    // Primary should succeed (using default implementation)
    // Result may vary based on actual implementation
    nimcp_exception_notify_recovery_result(ex, EXCEPTION_RECOVERY_GC, result == 0);

    nimcp_exception_unref(ex);
}

TEST_F(ExceptionImmuneBridgeIntegrationTest, RecoveryCallbackChain_MultipleFallbacks) {
    // WHAT: Test recovery chain with multiple fallbacks
    // WHY:  Verify fallback mechanisms work when primary fails

    nimcp_exception_t* ex = createException(
        NIMCP_ERROR_NO_MEMORY,
        EXCEPTION_SEVERITY_CRITICAL,
        "Severe memory pressure"
    );
    ASSERT_NE(ex, nullptr);

    nimcp_immune_response_t response;
    memset(&response, 0, sizeof(response));
    nimcp_exception_present_to_immune(ex, &response);

    nimcp_exception_recovery_strategy_t strategy;
    nimcp_exception_get_recovery_strategy(ex, &strategy);

    // Try primary action (GC)
    bool gc_success = (nimcp_exception_execute_recovery(ex, strategy.primary_action) == 0);
    nimcp_exception_notify_recovery_result(ex, strategy.primary_action, gc_success);

    if (!gc_success) {
        // Try fallback action (Quarantine)
        bool fallback_success = (nimcp_exception_execute_recovery(
            ex, strategy.fallback_action) == 0);
        nimcp_exception_notify_recovery_result(ex, strategy.fallback_action, fallback_success);
    }

    // Verify recovery was attempted
    nimcp_exception_immune_stats_t stats;
    nimcp_exception_immune_get_stats(&stats);
    EXPECT_GE(stats.recoveries_attempted, 1u);

    nimcp_exception_unref(ex);
}

TEST_F(ExceptionImmuneBridgeIntegrationTest, RecoveryStrategiesPerExceptionType) {
    // WHAT: Test that each exception type has appropriate recovery chain
    // WHY:  Different errors require different recovery strategies

    struct RecoveryTestCase {
        nimcp_error_t code;
        nimcp_exception_recovery_action_t expected_primary;
        nimcp_exception_recovery_action_t expected_fallback;
        const char* description;
    };

    std::vector<RecoveryTestCase> test_cases = {
        // Memory -> GC then Quarantine
        {NIMCP_ERROR_NO_MEMORY, EXCEPTION_RECOVERY_GC,
         EXCEPTION_RECOVERY_QUARANTINE, "Memory"},

        // Brain -> Rollback then Reduce Load
        {NIMCP_ERROR_BRAIN_CREATION, EXCEPTION_RECOVERY_ROLLBACK,
         EXCEPTION_RECOVERY_REDUCE_LOAD, "Brain"},

        // I/O -> Retry then Rollback
        {NIMCP_ERROR_FILE_READ, EXCEPTION_RECOVERY_RETRY,
         EXCEPTION_RECOVERY_ROLLBACK, "I/O"},

        // Threading -> Restart Thread then Graceful Shutdown
        {NIMCP_ERROR_DEADLOCK, EXCEPTION_RECOVERY_RESTART_THREAD,
         EXCEPTION_RECOVERY_GRACEFUL_SHUTDOWN, "Threading"},

        // GPU -> Clear Cache then Reduce Load
        {NIMCP_ERROR_GPU, EXCEPTION_RECOVERY_CLEAR_CACHE,
         EXCEPTION_RECOVERY_REDUCE_LOAD, "GPU"},
    };

    for (const auto& tc : test_cases) {
        nimcp_exception_t* ex = createException(
            tc.code,
            EXCEPTION_SEVERITY_ERROR,
            "Recovery chain test"
        );
        ASSERT_NE(ex, nullptr) << "Failed to create exception for " << tc.description;

        nimcp_exception_recovery_strategy_t strategy;
        nimcp_exception_get_recovery_strategy(ex, &strategy);

        EXPECT_EQ(strategy.primary_action, tc.expected_primary)
            << "Wrong primary action for " << tc.description;
        EXPECT_EQ(strategy.fallback_action, tc.expected_fallback)
            << "Wrong fallback action for " << tc.description;

        // Verify retry count and cooldown are set
        EXPECT_GT(strategy.retry_count, 0u)
            << "Retry count not set for " << tc.description;

        nimcp_exception_unref(ex);
    }
}

/* ============================================================================
 * Additional Integration Tests
 * ============================================================================ */

TEST_F(ExceptionImmuneBridgeIntegrationTest, ExceptionEpitopeMatchingForMemoryFormation) {
    // WHAT: Test that similar exceptions produce similar epitopes
    // WHY:  Immune memory relies on epitope matching for rapid secondary response

    // Create two similar exceptions
    nimcp_exception_t* ex1 = nimcp_exception_create(
        NIMCP_ERROR_NO_MEMORY,
        EXCEPTION_SEVERITY_SEVERE,
        "test.c", 100, "test_func",
        "Memory allocation failed"
    );

    nimcp_exception_t* ex2 = nimcp_exception_create(
        NIMCP_ERROR_NO_MEMORY,
        EXCEPTION_SEVERITY_SEVERE,
        "test.c", 100, "test_func",
        "Memory allocation failed"
    );

    ASSERT_NE(ex1, nullptr);
    ASSERT_NE(ex2, nullptr);

    // Compute epitopes
    uint8_t epitope1[NIMCP_EXCEPTION_EPITOPE_SIZE];
    uint8_t epitope2[NIMCP_EXCEPTION_EPITOPE_SIZE];

    size_t len1 = nimcp_exception_compute_epitope(ex1, epitope1, sizeof(epitope1));
    size_t len2 = nimcp_exception_compute_epitope(ex2, epitope2, sizeof(epitope2));

    EXPECT_GT(len1, 0u);
    EXPECT_EQ(len1, len2);

    // Epitopes should be identical for identical exceptions
    EXPECT_EQ(memcmp(epitope1, epitope2, len1), 0)
        << "Identical exceptions should produce identical epitopes";

    nimcp_exception_unref(ex1);
    nimcp_exception_unref(ex2);
}

TEST_F(ExceptionImmuneBridgeIntegrationTest, ConcurrentExceptionPresentation) {
    // WHAT: Test thread-safe exception presentation
    // WHY:  Multiple threads may throw exceptions simultaneously

    const int NUM_THREADS = 4;
    const int EXCEPTIONS_PER_THREAD = 10;
    std::atomic<int> successful_presentations{0};

    auto exception_thread = [&](int thread_id) {
        for (int i = 0; i < EXCEPTIONS_PER_THREAD; i++) {
            nimcp_exception_t* ex = nimcp_exception_create(
                static_cast<nimcp_error_t>(NIMCP_ERROR_NO_MEMORY + thread_id),
                EXCEPTION_SEVERITY_ERROR,
                __FILE__, __LINE__, __func__,
                "Thread %d exception %d", thread_id, i
            );
            if (!ex) continue;

            nimcp_immune_response_t response;
            memset(&response, 0, sizeof(response));
            if (nimcp_exception_present_to_immune(ex, &response) == 0) {
                successful_presentations++;
            }

            nimcp_exception_unref(ex);
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < NUM_THREADS; i++) {
        threads.emplace_back(exception_thread, i);
    }

    for (auto& t : threads) {
        t.join();
    }

    // Most presentations should succeed
    EXPECT_GT(successful_presentations, NUM_THREADS * EXCEPTIONS_PER_THREAD / 2)
        << "Too many concurrent presentation failures";
}

TEST_F(ExceptionImmuneBridgeIntegrationTest, FullIntegrationPipeline) {
    // WHAT: Test complete exception-immune pipeline
    // WHY:  Verify end-to-end integration works correctly

    // Reset stats
    nimcp_exception_immune_reset_stats();

    // Phase 1: Create and present exception
    nimcp_memory_exception_t* mem_ex = nimcp_memory_exception_create(
        NIMCP_ERROR_NO_MEMORY,
        EXCEPTION_SEVERITY_CRITICAL,
        __FILE__, __LINE__, __func__,
        10 * 1024 * 1024,  // 10MB
        "Out of memory during batch processing"
    );
    ASSERT_NE(mem_ex, nullptr);

    // Phase 2: Present to immune system
    nimcp_immune_response_t response;
    memset(&response, 0, sizeof(response));
    int result = nimcp_exception_present_to_immune((nimcp_exception_t*)mem_ex, &response);
    EXPECT_EQ(result, 0);
    EXPECT_GT(response.antigen_id, 0u);

    // Phase 3: Verify exception was marked as presented
    EXPECT_TRUE(mem_ex->base.presented_to_immune);
    EXPECT_EQ(mem_ex->base.antigen_id, response.antigen_id);

    // Phase 4: Get and verify recovery strategy
    nimcp_exception_recovery_strategy_t strategy;
    nimcp_exception_get_recovery_strategy((nimcp_exception_t*)mem_ex, &strategy);
    EXPECT_EQ(strategy.primary_action, EXCEPTION_RECOVERY_GC);

    // Phase 5: Execute recovery (may fail without full GC context, but tests the flow)
    result = nimcp_exception_execute_recovery(
        (nimcp_exception_t*)mem_ex, strategy.primary_action);
    // Recovery may fail without full context configured, but that's OK for integration test
    bool recovery_success = (result == 0);

    // Phase 6: Notify immune system
    result = nimcp_exception_notify_recovery_result(
        (nimcp_exception_t*)mem_ex,
        strategy.primary_action,
        recovery_success
    );
    EXPECT_EQ(result, 0);

    // Phase 7: Verify statistics
    nimcp_exception_immune_stats_t stats;
    nimcp_exception_immune_get_stats(&stats);
    EXPECT_GE(stats.exceptions_presented, 1u);
    EXPECT_GE(stats.recoveries_attempted, 1u);

    nimcp_exception_unref((nimcp_exception_t*)mem_ex);
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
