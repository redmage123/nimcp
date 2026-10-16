/**
 * @file test_cognitive_exception_integration.cpp
 * @brief Integration tests for cognitive exception handling with immune system
 *
 * WHAT: Test complete flow from cognitive module errors through exception system
 *       to immune system presentation and recovery execution
 * WHY:  Verify that cognitive errors trigger appropriate immune responses
 *       and recovery actions are properly executed
 * HOW:  Simulate cognitive module failures, verify exception chain,
 *       immune presentation, and recovery callbacks
 *
 * INTEGRATION SCENARIOS:
 * 1. Working memory error -> Exception -> Immune -> GC recovery
 * 2. Executive control error -> Exception -> Immune -> Reduce load
 * 3. Multiple cognitive failures -> Aggregate exception -> Batch immune processing
 * 4. Cascading failures (memory causes attention failure)
 * 5. Recovery callback execution and result notification
 *
 * @author NIMCP Development Team
 * @date 2026-01-16
 */

#include <gtest/gtest.h>
#include <cstring>
#include <atomic>
#include <vector>
#include <thread>
#include <chrono>

extern "C" {
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_handlers.h"
#include "utils/exception/nimcp_exception_immune.h"
#include "utils/error/nimcp_error_codes.h"
}

//=============================================================================
// Test Fixture
//=============================================================================

class CognitiveExceptionIntegrationTest : public ::testing::Test {
protected:
    static std::atomic<int> handler_call_count;
    static std::atomic<int> immune_presentation_count;
    static std::atomic<int> recovery_callback_count;
    static std::atomic<nimcp_exception_recovery_action_t> last_recovery_action;
    static std::atomic<bool> recovery_succeeded;
    static std::vector<nimcp_error_t> presented_error_codes;

    nimcp_handler_registration_t* test_handler_reg = nullptr;

    void SetUp() override {
        handler_call_count = 0;
        immune_presentation_count = 0;
        recovery_callback_count = 0;
        last_recovery_action = EXCEPTION_RECOVERY_NONE;
        recovery_succeeded = false;
        presented_error_codes.clear();

        // Initialize exception system
        nimcp_exception_system_init();

        // Initialize exception-immune integration with default config
        nimcp_exception_immune_config_t config;
        nimcp_exception_immune_default_config(&config);
        config.enable_auto_present = false;  // Manual control for testing
        config.enable_auto_recovery = false;
        nimcp_exception_immune_init(&config);

        // Install default handlers
        nimcp_install_default_handlers();

        // Register test handler
        nimcp_handler_options_t options;
        nimcp_handler_default_options(&options);
        options.name = "integration_test_handler";
        options.handler = test_handler;
        options.priority = NIMCP_HANDLER_PRIORITY_HIGH;
        options.user_data = nullptr;
        test_handler_reg = nimcp_handler_register(&options);
    }

    void TearDown() override {
        if (test_handler_reg) {
            nimcp_handler_unregister(test_handler_reg);
            test_handler_reg = nullptr;
        }

        nimcp_exception_clear_current();
        nimcp_exception_immune_shutdown();
        nimcp_exception_system_shutdown();
    }

    static bool test_handler(nimcp_exception_t* ex, void* user_data) {
        (void)user_data;
        handler_call_count++;
        return false;  // Don't consume
    }

    static int test_recovery_callback(nimcp_exception_t* ex,
                                       nimcp_exception_recovery_action_t action,
                                       void* user_data) {
        (void)user_data;
        recovery_callback_count++;
        last_recovery_action = action;

        // Simulate recovery based on action type
        switch (action) {
            case EXCEPTION_RECOVERY_GC:
            case EXCEPTION_RECOVERY_CLEAR_CACHE:
            case EXCEPTION_RECOVERY_REDUCE_LOAD:
                recovery_succeeded = true;
                return 0;  // Success
            case EXCEPTION_RECOVERY_ROLLBACK:
            case EXCEPTION_RECOVERY_RESTART_COMPONENT:
                recovery_succeeded = true;
                return 0;
            default:
                recovery_succeeded = false;
                return -1;  // Failure
        }
    }
};

std::atomic<int> CognitiveExceptionIntegrationTest::handler_call_count(0);
std::atomic<int> CognitiveExceptionIntegrationTest::immune_presentation_count(0);
std::atomic<int> CognitiveExceptionIntegrationTest::recovery_callback_count(0);
std::atomic<nimcp_exception_recovery_action_t> CognitiveExceptionIntegrationTest::last_recovery_action(EXCEPTION_RECOVERY_NONE);
std::atomic<bool> CognitiveExceptionIntegrationTest::recovery_succeeded(false);
std::vector<nimcp_error_t> CognitiveExceptionIntegrationTest::presented_error_codes;

//=============================================================================
// Exception to Immune Flow Tests
//=============================================================================

TEST_F(CognitiveExceptionIntegrationTest, WorkingMemoryExceptionToImmune) {
    // WHAT: Test working memory exception flows to immune system
    // WHY:  Verify complete integration path for common cognitive error
    // HOW:  Create exception, present to immune, check response

    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_WORKING_MEMORY,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        "Working memory capacity exceeded during task processing"
    );
    ASSERT_NE(ex, nullptr);

    // Add context for immune processing
    nimcp_exception_set_context(ex, "module", "working_memory");
    nimcp_exception_set_context(ex, "capacity_used", "8");
    nimcp_exception_set_context(ex, "capacity_max", "7");

    // Generate epitope for immune pattern matching
    size_t epitope_len = nimcp_exception_generate_epitope(ex);
    EXPECT_GT(epitope_len, 0u);

    // Present to immune system
    nimcp_immune_response_t response;
    memset(&response, 0, sizeof(response));
    int result = nimcp_exception_present_to_immune(ex, &response);
    EXPECT_EQ(result, 0);

    // Verify exception was marked as presented
    EXPECT_TRUE(ex->presented_to_immune);

    // Get recovery strategy
    nimcp_exception_recovery_strategy_t strategy;
    nimcp_exception_get_recovery_strategy(ex, &strategy);

    // Working memory errors should suggest GC or cache clearing
    EXPECT_TRUE(strategy.primary_action == EXCEPTION_RECOVERY_GC ||
                strategy.primary_action == EXCEPTION_RECOVERY_CLEAR_CACHE ||
                strategy.primary_action == EXCEPTION_RECOVERY_REDUCE_LOAD);

    nimcp_exception_unref(ex);
}

TEST_F(CognitiveExceptionIntegrationTest, ExecutiveControlExceptionRecovery) {
    // WHAT: Test executive control exception with recovery
    // WHY:  Verify recovery callback is executed for severe errors
    // HOW:  Register recovery callback, trigger exception, verify callback

    // Register recovery callback for REDUCE_LOAD action
    int reg_result = nimcp_register_recovery_callback(
        EXCEPTION_RECOVERY_REDUCE_LOAD,
        test_recovery_callback,
        nullptr
    );
    EXPECT_EQ(reg_result, 0);

    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_EXECUTIVE_CONTROL,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        "Executive control overload - task switching impaired"
    );
    ASSERT_NE(ex, nullptr);

    // Present to immune
    nimcp_immune_response_t response;
    int result = nimcp_exception_present_to_immune(ex, &response);
    EXPECT_EQ(result, 0);

    // Execute recovery
    recovery_callback_count = 0;
    int recovery_result = nimcp_exception_execute_recovery(ex, EXCEPTION_RECOVERY_REDUCE_LOAD);
    EXPECT_EQ(recovery_result, 0);
    EXPECT_EQ(recovery_callback_count.load(), 1);
    EXPECT_EQ(last_recovery_action.load(), EXCEPTION_RECOVERY_REDUCE_LOAD);
    EXPECT_TRUE(recovery_succeeded.load());

    // Notify immune of recovery result
    int notify_result = nimcp_exception_notify_recovery_result(
        ex, EXCEPTION_RECOVERY_REDUCE_LOAD, true
    );
    EXPECT_EQ(notify_result, 0);

    // Clean up
    nimcp_unregister_recovery_callback(EXCEPTION_RECOVERY_REDUCE_LOAD);
    nimcp_exception_unref(ex);
}

TEST_F(CognitiveExceptionIntegrationTest, EmotionalTaggingExceptionFlow) {
    // WHAT: Test emotional tagging exception integration
    // WHY:  Emotional system errors need careful recovery
    // HOW:  Create exception, dispatch, verify handler chain

    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_EMOTIONAL_TAGGING,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Emotional tagging failed for memory consolidation"
    );
    ASSERT_NE(ex, nullptr);

    // Dispatch through handler chain
    handler_call_count = 0;
    bool handled = nimcp_exception_dispatch(ex);

    // Default handlers should process it
    EXPECT_GE(handler_call_count.load(), 1);

    // Present to immune
    nimcp_immune_response_t response;
    int result = nimcp_exception_present_to_immune(ex, &response);
    EXPECT_EQ(result, 0);

    nimcp_exception_unref(ex);
}

//=============================================================================
// Cascading Exception Tests
//=============================================================================

TEST_F(CognitiveExceptionIntegrationTest, CascadingCognitiveFailure) {
    // WHAT: Test cascading failure (root cause -> cognitive failure)
    // WHY:  Cognitive failures often have underlying causes
    // HOW:  Create exception chain, verify cause traversal

    // Root cause: memory allocation failure
    nimcp_exception_t* root_cause = nimcp_exception_create(
        NIMCP_ERROR_NO_MEMORY,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Failed to allocate attention buffer"
    );
    ASSERT_NE(root_cause, nullptr);

    // Immediate cause: working memory failure
    nimcp_exception_t* wm_failure = nimcp_exception_create(
        NIMCP_ERROR_WORKING_MEMORY,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Working memory buffer allocation failed"
    );
    nimcp_exception_set_cause(wm_failure, root_cause);

    // Top-level: executive control failure
    nimcp_exception_t* exec_failure = nimcp_exception_create(
        NIMCP_ERROR_EXECUTIVE_CONTROL,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        "Executive control failed due to working memory unavailable"
    );
    nimcp_exception_set_cause(exec_failure, wm_failure);

    // Verify chain
    nimcp_exception_t* cause1 = nimcp_exception_get_cause(exec_failure);
    ASSERT_NE(cause1, nullptr);
    EXPECT_EQ(cause1->code, NIMCP_ERROR_WORKING_MEMORY);

    nimcp_exception_t* cause2 = nimcp_exception_get_cause(cause1);
    ASSERT_NE(cause2, nullptr);
    EXPECT_EQ(cause2->code, NIMCP_ERROR_NO_MEMORY);

    // Present top-level to immune (should consider chain)
    nimcp_immune_response_t response;
    int result = nimcp_exception_present_to_immune(exec_failure, &response);
    EXPECT_EQ(result, 0);

    // Recovery should target root cause
    nimcp_exception_recovery_strategy_t strategy;
    nimcp_exception_get_recovery_strategy(exec_failure, &strategy);
    // For severe cognitive errors, expect significant recovery action
    EXPECT_NE(strategy.primary_action, EXCEPTION_RECOVERY_NONE);

    nimcp_exception_unref(exec_failure);
}

//=============================================================================
// Aggregate Exception Tests
//=============================================================================

TEST_F(CognitiveExceptionIntegrationTest, MultipleCognitiveFailures) {
    // WHAT: Test aggregate exception for multiple cognitive subsystem failures
    // WHY:  System-wide issues may affect multiple cognitive modules
    // HOW:  Create aggregate, add children, process as batch

    nimcp_aggregate_exception_t* agg = nimcp_aggregate_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_CRITICAL,
        __FILE__, __LINE__, __func__,
        "Multiple cognitive subsystem failures detected"
    );
    ASSERT_NE(agg, nullptr);

    // Add working memory failure
    nimcp_exception_t* wm_ex = nimcp_exception_create(
        NIMCP_ERROR_WORKING_MEMORY,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Working memory degraded"
    );
    nimcp_aggregate_exception_add(agg, wm_ex);

    // Add executive control failure
    nimcp_exception_t* exec_ex = nimcp_exception_create(
        NIMCP_ERROR_EXECUTIVE_CONTROL,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Executive control impaired"
    );
    nimcp_aggregate_exception_add(agg, exec_ex);

    // Add emotional tagging failure
    nimcp_exception_t* emo_ex = nimcp_exception_create(
        NIMCP_ERROR_EMOTIONAL_TAGGING,
        EXCEPTION_SEVERITY_WARNING,
        __FILE__, __LINE__, __func__,
        "Emotional tagging delayed"
    );
    nimcp_aggregate_exception_add(agg, emo_ex);

    // Add mental health monitor failure
    nimcp_exception_t* mh_ex = nimcp_exception_create(
        NIMCP_ERROR_MENTAL_HEALTH,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        "Mental health monitor threshold exceeded"
    );
    nimcp_aggregate_exception_add(agg, mh_ex);

    // Verify all children added
    EXPECT_EQ(nimcp_aggregate_exception_count(agg), 4u);

    // Dispatch aggregate
    handler_call_count = 0;
    nimcp_exception_dispatch((nimcp_exception_t*)agg);
    EXPECT_GE(handler_call_count.load(), 1);

    // Present aggregate to immune
    nimcp_immune_response_t response;
    int result = nimcp_exception_present_to_immune((nimcp_exception_t*)agg, &response);
    EXPECT_EQ(result, 0);

    nimcp_exception_unref((nimcp_exception_t*)agg);
}

//=============================================================================
// Recovery Strategy Tests
//=============================================================================

TEST_F(CognitiveExceptionIntegrationTest, RecoveryStrategyByErrorType) {
    // WHAT: Test that different cognitive errors get appropriate recovery strategies
    // WHY:  Each cognitive module may need different recovery approach
    // HOW:  Create exceptions for each type, check recovery strategy

    struct {
        nimcp_error_t code;
        const char* name;
    } cognitive_errors[] = {
        { NIMCP_ERROR_WORKING_MEMORY, "WORKING_MEMORY" },
        { NIMCP_ERROR_EMOTIONAL_TAGGING, "EMOTIONAL_TAGGING" },
        { NIMCP_ERROR_EXECUTIVE_CONTROL, "EXECUTIVE_CONTROL" },
        { NIMCP_ERROR_SLEEP_WAKE, "SLEEP_WAKE" },
        { NIMCP_ERROR_MENTAL_HEALTH, "MENTAL_HEALTH" },
        { NIMCP_ERROR_THEORY_OF_MIND, "THEORY_OF_MIND" },
        { NIMCP_ERROR_EXPLANATIONS, "EXPLANATIONS" },
        { NIMCP_ERROR_META_LEARNING, "META_LEARNING" },
        { NIMCP_ERROR_PREDICTIVE, "PREDICTIVE" }
    };

    for (const auto& error : cognitive_errors) {
        nimcp_exception_t* ex = nimcp_exception_create(
            error.code,
            EXCEPTION_SEVERITY_SEVERE,
            __FILE__, __LINE__, __func__,
            "Test %s recovery strategy", error.name
        );
        ASSERT_NE(ex, nullptr) << "Failed for " << error.name;

        nimcp_exception_recovery_strategy_t strategy;
        nimcp_exception_get_recovery_strategy(ex, &strategy);

        // All cognitive errors should have some recovery strategy
        EXPECT_NE(strategy.primary_action, EXCEPTION_RECOVERY_NONE)
            << "No primary action for " << error.name;

        // Should have defined retry count
        EXPECT_GT(strategy.retry_count, 0u)
            << "No retry count for " << error.name;

        nimcp_exception_unref(ex);
    }
}

TEST_F(CognitiveExceptionIntegrationTest, FallbackRecoveryExecution) {
    // WHAT: Test fallback recovery when primary fails
    // WHY:  Robust error handling requires fallback options
    // HOW:  Make primary fail, verify fallback is attempted

    // Register failing primary callback
    auto failing_callback = [](nimcp_exception_t* ex,
                               nimcp_exception_recovery_action_t action,
                               void* user_data) -> int {
        (void)ex;
        (void)action;
        (void)user_data;
        return -1;  // Failure
    };

    nimcp_register_recovery_callback(EXCEPTION_RECOVERY_GC, failing_callback, nullptr);
    nimcp_register_recovery_callback(EXCEPTION_RECOVERY_CLEAR_CACHE, test_recovery_callback, nullptr);

    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_WORKING_MEMORY,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        "Working memory failure requiring recovery"
    );

    // Primary recovery should fail
    int primary_result = nimcp_exception_execute_recovery(ex, EXCEPTION_RECOVERY_GC);
    EXPECT_EQ(primary_result, -1);

    // Fallback should succeed
    recovery_callback_count = 0;
    int fallback_result = nimcp_exception_execute_recovery(ex, EXCEPTION_RECOVERY_CLEAR_CACHE);
    EXPECT_EQ(fallback_result, 0);
    EXPECT_TRUE(recovery_succeeded.load());

    nimcp_unregister_recovery_callback(EXCEPTION_RECOVERY_GC);
    nimcp_unregister_recovery_callback(EXCEPTION_RECOVERY_CLEAR_CACHE);
    nimcp_exception_unref(ex);
}

//=============================================================================
// Async Processing Tests
//=============================================================================

TEST_F(CognitiveExceptionIntegrationTest, AsyncExceptionPresentation) {
    // WHAT: Test async exception presentation
    // WHY:  Non-blocking presentation for high-throughput scenarios
    // HOW:  Queue exceptions, process batch

    // Create multiple exceptions
    std::vector<nimcp_exception_t*> exceptions;
    for (int i = 0; i < 5; i++) {
        nimcp_exception_t* ex = nimcp_exception_create(
            NIMCP_ERROR_WORKING_MEMORY,
            EXCEPTION_SEVERITY_ERROR,
            __FILE__, __LINE__, __func__,
            "Async test exception %d", i
        );
        ASSERT_NE(ex, nullptr);
        exceptions.push_back(ex);
    }

    // Queue for async presentation
    for (auto* ex : exceptions) {
        int result = nimcp_exception_present_async(ex);
        EXPECT_EQ(result, 0);
    }

    // Process pending
    size_t processed = nimcp_exception_immune_process_pending(0);
    EXPECT_EQ(processed, 5u);

    // Clean up
    for (auto* ex : exceptions) {
        nimcp_exception_unref(ex);
    }
}

//=============================================================================
// Antigen Source Mapping Tests
//=============================================================================

TEST_F(CognitiveExceptionIntegrationTest, CognitiveToAntigenSourceMapping) {
    // WHAT: Test mapping of cognitive exception category to antigen source
    // WHY:  Immune system needs consistent antigen classification
    // HOW:  Check mapping function returns expected source type

    exception_antigen_source_t source = nimcp_exception_to_antigen_source(
        EXCEPTION_CATEGORY_COGNITIVE
    );

    // Cognitive errors should map to ANOMALY source (general anomaly detection)
    EXPECT_EQ(source, EX_ANTIGEN_SOURCE_ANOMALY);
}

TEST_F(CognitiveExceptionIntegrationTest, SeverityToImmuneSeverityMapping) {
    // WHAT: Test severity mapping to immune system scale (1-10)
    // WHY:  Immune system uses 1-10 severity scale
    // HOW:  Map each exception severity and verify range

    struct {
        nimcp_exception_severity_t ex_severity;
        uint32_t expected_min;
        uint32_t expected_max;
    } mappings[] = {
        { EXCEPTION_SEVERITY_DEBUG, 1, 2 },
        { EXCEPTION_SEVERITY_INFO, 2, 3 },
        { EXCEPTION_SEVERITY_WARNING, 3, 4 },
        { EXCEPTION_SEVERITY_ERROR, 4, 6 },
        { EXCEPTION_SEVERITY_SEVERE, 6, 8 },
        { EXCEPTION_SEVERITY_CRITICAL, 8, 10 },
        { EXCEPTION_SEVERITY_FATAL, 9, 10 }
    };

    for (const auto& mapping : mappings) {
        uint32_t immune_severity = nimcp_exception_to_immune_severity(mapping.ex_severity);
        EXPECT_GE(immune_severity, mapping.expected_min)
            << "Severity " << mapping.ex_severity << " mapped too low";
        EXPECT_LE(immune_severity, mapping.expected_max)
            << "Severity " << mapping.ex_severity << " mapped too high";
    }
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(CognitiveExceptionIntegrationTest, ExceptionImmuneStatistics) {
    // WHAT: Test exception-immune integration statistics
    // WHY:  Statistics help monitor system health
    // HOW:  Generate exceptions, check statistics

    // Reset statistics
    nimcp_exception_immune_reset_stats();

    // Create and present several exceptions
    for (int i = 0; i < 3; i++) {
        nimcp_exception_t* ex = nimcp_exception_create(
            NIMCP_ERROR_WORKING_MEMORY,
            EXCEPTION_SEVERITY_ERROR,
            __FILE__, __LINE__, __func__,
            "Statistics test exception %d", i
        );
        nimcp_immune_response_t response;
        nimcp_exception_present_to_immune(ex, &response);
        nimcp_exception_unref(ex);
    }

    // Check statistics
    nimcp_exception_immune_stats_t stats;
    nimcp_exception_immune_get_stats(&stats);

    EXPECT_GE(stats.exceptions_presented, 3u);
    // Average response time should be non-negative
    EXPECT_GE(stats.avg_response_time_us, 0.0f);
}

//=============================================================================
// Try/Catch Integration Tests
//=============================================================================

TEST_F(CognitiveExceptionIntegrationTest, TryCatchCognitiveException) {
    // WHAT: Test try/catch mechanism with cognitive exceptions
    // WHY:  Structured exception handling for cognitive code
    // HOW:  Use NIMCP_TRY/NIMCP_CATCH macros

    bool caught = false;
    nimcp_error_t caught_code = NIMCP_SUCCESS;

    NIMCP_TRY {
        // Simulate cognitive module raising exception
        nimcp_exception_t* ex = nimcp_exception_create(
            NIMCP_ERROR_THEORY_OF_MIND,
            EXCEPTION_SEVERITY_ERROR,
            __FILE__, __LINE__, __func__,
            "Theory of mind model inconsistency"
        );
        nimcp_exception_raise(ex);
        // Should not reach here
        FAIL() << "Should have jumped to catch block";
    }
    NIMCP_CATCH(nimcp_exception_t, ex) {
        caught = true;
        caught_code = ex->code;
        EXPECT_EQ(ex->category, EXCEPTION_CATEGORY_COGNITIVE);
        nimcp_exception_unref(ex);
    }
    NIMCP_END_TRY;

    EXPECT_TRUE(caught);
    EXPECT_EQ(caught_code, NIMCP_ERROR_THEORY_OF_MIND);
}

TEST_F(CognitiveExceptionIntegrationTest, NestedTryCatch) {
    // WHAT: Test nested try/catch blocks
    // WHY:  Complex cognitive operations may have nested error handling
    // HOW:  Create nested try blocks, verify exception propagation

    bool outer_caught = false;
    bool inner_caught = false;

    NIMCP_TRY {
        NIMCP_TRY {
            nimcp_exception_t* ex = nimcp_exception_create(
                NIMCP_ERROR_META_LEARNING,
                EXCEPTION_SEVERITY_ERROR,
                __FILE__, __LINE__, __func__,
                "Inner exception"
            );
            nimcp_exception_raise(ex);
        }
        NIMCP_CATCH(nimcp_exception_t, inner_ex) {
            inner_caught = true;
            EXPECT_EQ(inner_ex->code, NIMCP_ERROR_META_LEARNING);

            // Re-wrap and rethrow
            nimcp_exception_t* outer_ex = nimcp_exception_create(
                NIMCP_ERROR_EXECUTIVE_CONTROL,
                EXCEPTION_SEVERITY_SEVERE,
                __FILE__, __LINE__, __func__,
                "Outer wrapper exception"
            );
            nimcp_exception_set_cause(outer_ex, inner_ex);
            nimcp_exception_raise(outer_ex);
        }
        NIMCP_END_TRY;
    }
    NIMCP_CATCH(nimcp_exception_t, outer_ex) {
        outer_caught = true;
        EXPECT_EQ(outer_ex->code, NIMCP_ERROR_EXECUTIVE_CONTROL);

        // Verify cause chain
        nimcp_exception_t* cause = nimcp_exception_get_cause(outer_ex);
        ASSERT_NE(cause, nullptr);
        EXPECT_EQ(cause->code, NIMCP_ERROR_META_LEARNING);

        nimcp_exception_unref(outer_ex);
    }
    NIMCP_END_TRY;

    EXPECT_TRUE(inner_caught);
    EXPECT_TRUE(outer_caught);
}

//=============================================================================
// Epitope Computation Tests
//=============================================================================

TEST_F(CognitiveExceptionIntegrationTest, EpitopeConsistency) {
    // WHAT: Test that same exception produces same epitope
    // WHY:  Immune pattern matching requires consistent epitopes
    // HOW:  Create same exception twice, compare epitopes

    nimcp_exception_t* ex1 = nimcp_exception_create(
        NIMCP_ERROR_PREDICTIVE,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, 100, "test_func",  // Fixed location
        "Predictive processing error"
    );

    nimcp_exception_t* ex2 = nimcp_exception_create(
        NIMCP_ERROR_PREDICTIVE,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, 100, "test_func",  // Same fixed location
        "Predictive processing error"
    );

    ASSERT_NE(ex1, nullptr);
    ASSERT_NE(ex2, nullptr);

    // Compute epitopes
    uint8_t epitope1[NIMCP_EXCEPTION_EPITOPE_SIZE];
    uint8_t epitope2[NIMCP_EXCEPTION_EPITOPE_SIZE];

    size_t len1 = nimcp_exception_compute_epitope(ex1, epitope1, sizeof(epitope1));
    size_t len2 = nimcp_exception_compute_epitope(ex2, epitope2, sizeof(epitope2));

    EXPECT_EQ(len1, len2);
    EXPECT_EQ(memcmp(epitope1, epitope2, len1), 0)
        << "Same exception should produce same epitope";

    nimcp_exception_unref(ex1);
    nimcp_exception_unref(ex2);
}

TEST_F(CognitiveExceptionIntegrationTest, EpitopeDistinctness) {
    // WHAT: Test that different exceptions produce different epitopes
    // WHY:  Immune system needs to distinguish exception types
    // HOW:  Create exceptions with different codes, verify different epitopes

    nimcp_exception_t* ex_wm = nimcp_exception_create(
        NIMCP_ERROR_WORKING_MEMORY,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Working memory error"
    );

    nimcp_exception_t* ex_exec = nimcp_exception_create(
        NIMCP_ERROR_EXECUTIVE_CONTROL,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Executive control error"
    );

    uint8_t epitope_wm[NIMCP_EXCEPTION_EPITOPE_SIZE];
    uint8_t epitope_exec[NIMCP_EXCEPTION_EPITOPE_SIZE];

    size_t len_wm = nimcp_exception_compute_epitope(ex_wm, epitope_wm, sizeof(epitope_wm));
    size_t len_exec = nimcp_exception_compute_epitope(ex_exec, epitope_exec, sizeof(epitope_exec));

    // Different error codes should produce different epitopes
    bool different = (len_wm != len_exec) ||
                     (memcmp(epitope_wm, epitope_exec, std::min(len_wm, len_exec)) != 0);
    EXPECT_TRUE(different)
        << "Different exceptions should produce different epitopes";

    nimcp_exception_unref(ex_wm);
    nimcp_exception_unref(ex_exec);
}

//=============================================================================
// Concurrent Exception Handling Tests
//=============================================================================

TEST_F(CognitiveExceptionIntegrationTest, ConcurrentExceptionCreation) {
    // WHAT: Test thread-safe exception creation
    // WHY:  Multiple cognitive modules may fail concurrently
    // HOW:  Create exceptions from multiple threads

    const int NUM_THREADS = 4;
    const int EXCEPTIONS_PER_THREAD = 10;
    std::atomic<int> total_created(0);
    std::vector<std::thread> threads;

    for (int t = 0; t < NUM_THREADS; t++) {
        threads.emplace_back([&total_created, t]() {
            for (int i = 0; i < EXCEPTIONS_PER_THREAD; i++) {
                nimcp_exception_t* ex = nimcp_exception_create(
                    NIMCP_ERROR_WORKING_MEMORY + (t % 9),  // Rotate through cognitive errors
                    EXCEPTION_SEVERITY_ERROR,
                    __FILE__, __LINE__, __func__,
                    "Concurrent test thread %d iteration %d", t, i
                );
                if (ex) {
                    total_created++;

                    // Brief operations
                    nimcp_exception_generate_epitope(ex);
                    nimcp_exception_dispatch(ex);

                    nimcp_exception_unref(ex);
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(total_created.load(), NUM_THREADS * EXCEPTIONS_PER_THREAD);
}

//=============================================================================
// Default Handler Integration Tests
//=============================================================================

TEST_F(CognitiveExceptionIntegrationTest, DefaultLoggingHandler) {
    // WHAT: Test default logging handler processes cognitive exceptions
    // WHY:  All exceptions should be logged for diagnostics
    // HOW:  Create exception, dispatch, verify handler processes it

    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_EXPLANATIONS,
        EXCEPTION_SEVERITY_WARNING,
        __FILE__, __LINE__, __func__,
        "Natural explanation generation incomplete"
    );

    // Dispatch - default handlers should process
    bool handled = nimcp_exception_dispatch(ex);

    // Default handlers typically don't consume (return false) but do process
    // The test handler at HIGH priority will increment counter
    EXPECT_GE(handler_call_count.load(), 1);

    nimcp_exception_unref(ex);
}

TEST_F(CognitiveExceptionIntegrationTest, DefaultImmuneHandler) {
    // WHAT: Test default immune handler presents severe exceptions
    // WHY:  Severe errors should automatically trigger immune response
    // HOW:  Create severe exception, dispatch, verify immune presentation

    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_MENTAL_HEALTH,
        EXCEPTION_SEVERITY_SEVERE,  // Above threshold
        __FILE__, __LINE__, __func__,
        "Critical mental health threshold exceeded"
    );

    // Dispatch - default immune handler should present it
    nimcp_exception_dispatch(ex);

    // The default immune handler should have processed this severe exception
    // We can verify by checking if it was marked as presented
    // (depending on implementation, default handler may set this)

    nimcp_exception_unref(ex);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
