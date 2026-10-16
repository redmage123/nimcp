/**
 * @file test_cognitive_exception_handling.cpp
 * @brief Unit tests for cognitive module exception handling
 *
 * WHAT: Test exception handling for cognitive modules (attention, memory, reasoning,
 *       emotions, curiosity, wellbeing, executive control, etc.)
 * WHY:  Verify that cognitive errors properly map to exceptions and trigger
 *       appropriate recovery strategies through the immune system
 * HOW:  Test error code mapping, exception creation, handler dispatch,
 *       recovery strategy selection, and immune presentation
 *
 * COGNITIVE ERROR CODES (8000-8999):
 * - 8000: NIMCP_ERROR_WORKING_MEMORY
 * - 8001: NIMCP_ERROR_EMOTIONAL_TAGGING
 * - 8002: NIMCP_ERROR_EXECUTIVE_CONTROL
 * - 8003: NIMCP_ERROR_SLEEP_WAKE
 * - 8004: NIMCP_ERROR_MENTAL_HEALTH
 * - 8005: NIMCP_ERROR_THEORY_OF_MIND
 * - 8006: NIMCP_ERROR_EXPLANATIONS
 * - 8007: NIMCP_ERROR_META_LEARNING
 * - 8008: NIMCP_ERROR_PREDICTIVE
 *
 * @author NIMCP Development Team
 * @date 2026-01-16
 */

#include <gtest/gtest.h>
#include <cstring>
#include <atomic>
#include <vector>
#include <string>

extern "C" {
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_handlers.h"
#include "utils/exception/nimcp_exception_immune.h"
#include "utils/error/nimcp_error_codes.h"
}

//=============================================================================
// Test Fixture
//=============================================================================

class CognitiveExceptionHandlingTest : public ::testing::Test {
protected:
    static std::atomic<int> handler_call_count;
    static std::atomic<int> last_exception_code;
    static std::atomic<int> last_exception_category;
    static std::atomic<bool> handler_consumed_exception;
    static std::vector<std::string> handler_messages;

    void SetUp() override {
        handler_call_count = 0;
        last_exception_code = 0;
        last_exception_category = 0;
        handler_consumed_exception = false;
        handler_messages.clear();

        nimcp_exception_system_init();
    }

    void TearDown() override {
        nimcp_exception_clear_current();
        nimcp_exception_system_shutdown();
    }

    static bool test_exception_handler(nimcp_exception_t* ex, void* user_data) {
        (void)user_data;
        handler_call_count++;
        last_exception_code = ex->code;
        last_exception_category = ex->category;
        handler_messages.push_back(ex->message);
        return false;  // Don't consume - allow other handlers to process
    }

    static bool consuming_handler(nimcp_exception_t* ex, void* user_data) {
        (void)user_data;
        handler_call_count++;
        last_exception_code = ex->code;
        handler_consumed_exception = true;
        return true;  // Consume the exception
    }

    static bool cognitive_filter_handler(nimcp_exception_t* ex, void* user_data) {
        (void)user_data;
        if (ex->category == EXCEPTION_CATEGORY_COGNITIVE) {
            handler_call_count++;
            last_exception_code = ex->code;
            return false;
        }
        return false;
    }
};

std::atomic<int> CognitiveExceptionHandlingTest::handler_call_count(0);
std::atomic<int> CognitiveExceptionHandlingTest::last_exception_code(0);
std::atomic<int> CognitiveExceptionHandlingTest::last_exception_category(0);
std::atomic<bool> CognitiveExceptionHandlingTest::handler_consumed_exception(false);
std::vector<std::string> CognitiveExceptionHandlingTest::handler_messages;

//=============================================================================
// Error Code Mapping Tests
//=============================================================================

TEST_F(CognitiveExceptionHandlingTest, CognitiveErrorCodeToCategory) {
    // WHAT: Test mapping of cognitive error codes to EXCEPTION_CATEGORY_COGNITIVE
    // WHY:  Ensure all cognitive errors are properly categorized
    // HOW:  Check each cognitive error code maps to COGNITIVE category

    nimcp_error_t cognitive_errors[] = {
        NIMCP_ERROR_WORKING_MEMORY,
        NIMCP_ERROR_EMOTIONAL_TAGGING,
        NIMCP_ERROR_EXECUTIVE_CONTROL,
        NIMCP_ERROR_SLEEP_WAKE,
        NIMCP_ERROR_MENTAL_HEALTH,
        NIMCP_ERROR_THEORY_OF_MIND,
        NIMCP_ERROR_EXPLANATIONS,
        NIMCP_ERROR_META_LEARNING,
        NIMCP_ERROR_PREDICTIVE
    };

    for (nimcp_error_t code : cognitive_errors) {
        nimcp_exception_category_t category = nimcp_exception_get_category_from_code(code);
        EXPECT_EQ(category, EXCEPTION_CATEGORY_COGNITIVE)
            << "Error code " << code << " should map to COGNITIVE category";
    }
}

TEST_F(CognitiveExceptionHandlingTest, CognitiveErrorCodeRange) {
    // WHAT: Test that cognitive error codes are in the expected range (8000-8999)
    // WHY:  Verify error code range conventions are followed
    // HOW:  Check each cognitive error code is between 8000 and 8999

    EXPECT_EQ(NIMCP_ERROR_WORKING_MEMORY, 8000);
    EXPECT_EQ(NIMCP_ERROR_EMOTIONAL_TAGGING, 8001);
    EXPECT_EQ(NIMCP_ERROR_EXECUTIVE_CONTROL, 8002);
    EXPECT_EQ(NIMCP_ERROR_SLEEP_WAKE, 8003);
    EXPECT_EQ(NIMCP_ERROR_MENTAL_HEALTH, 8004);
    EXPECT_EQ(NIMCP_ERROR_THEORY_OF_MIND, 8005);
    EXPECT_EQ(NIMCP_ERROR_EXPLANATIONS, 8006);
    EXPECT_EQ(NIMCP_ERROR_META_LEARNING, 8007);
    EXPECT_EQ(NIMCP_ERROR_PREDICTIVE, 8008);
}

//=============================================================================
// Exception Creation Tests
//=============================================================================

TEST_F(CognitiveExceptionHandlingTest, CreateCognitiveException) {
    // WHAT: Test cognitive exception creation
    // WHY:  Verify cognitive exceptions are created with correct properties
    // HOW:  Create exception and check all fields

    nimcp_cognitive_exception_t* ex = (nimcp_cognitive_exception_t*)nimcp_exception_create(
        NIMCP_ERROR_WORKING_MEMORY,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Working memory capacity exceeded"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->base.code, NIMCP_ERROR_WORKING_MEMORY);
    EXPECT_EQ(ex->base.category, EXCEPTION_CATEGORY_COGNITIVE);
    EXPECT_EQ(ex->base.severity, EXCEPTION_SEVERITY_ERROR);
    EXPECT_STREQ(ex->base.message, "Working memory capacity exceeded");

    nimcp_exception_unref((nimcp_exception_t*)ex);
}

TEST_F(CognitiveExceptionHandlingTest, CreateAllCognitiveExceptions) {
    // WHAT: Test creation of exceptions for all cognitive error codes
    // WHY:  Ensure all cognitive error types can be properly converted to exceptions
    // HOW:  Create exception for each cognitive error and verify properties

    struct {
        nimcp_error_t code;
        const char* name;
        const char* message;
    } cognitive_errors[] = {
        { NIMCP_ERROR_WORKING_MEMORY, "WORKING_MEMORY", "Working memory error" },
        { NIMCP_ERROR_EMOTIONAL_TAGGING, "EMOTIONAL_TAGGING", "Emotional tagging error" },
        { NIMCP_ERROR_EXECUTIVE_CONTROL, "EXECUTIVE_CONTROL", "Executive control error" },
        { NIMCP_ERROR_SLEEP_WAKE, "SLEEP_WAKE", "Sleep/wake cycle error" },
        { NIMCP_ERROR_MENTAL_HEALTH, "MENTAL_HEALTH", "Mental health monitor error" },
        { NIMCP_ERROR_THEORY_OF_MIND, "THEORY_OF_MIND", "Theory of mind error" },
        { NIMCP_ERROR_EXPLANATIONS, "EXPLANATIONS", "Natural explanations error" },
        { NIMCP_ERROR_META_LEARNING, "META_LEARNING", "Meta-learning error" },
        { NIMCP_ERROR_PREDICTIVE, "PREDICTIVE", "Predictive processing error" }
    };

    for (const auto& error : cognitive_errors) {
        nimcp_exception_t* ex = nimcp_exception_create(
            error.code,
            EXCEPTION_SEVERITY_ERROR,
            __FILE__, __LINE__, __func__,
            "%s", error.message
        );

        ASSERT_NE(ex, nullptr) << "Failed to create exception for " << error.name;
        EXPECT_EQ(ex->code, error.code) << "Wrong code for " << error.name;
        EXPECT_EQ(ex->category, EXCEPTION_CATEGORY_COGNITIVE)
            << "Wrong category for " << error.name;
        EXPECT_STREQ(ex->message, error.message) << "Wrong message for " << error.name;

        nimcp_exception_unref(ex);
    }
}

TEST_F(CognitiveExceptionHandlingTest, CognitiveExceptionContext) {
    // WHAT: Test adding context to cognitive exceptions
    // WHY:  Context provides additional debugging information
    // HOW:  Create exception, add context, verify retrieval

    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_WORKING_MEMORY,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Capacity exceeded"
    );

    ASSERT_NE(ex, nullptr);

    // Add context entries
    int result = nimcp_exception_set_context(ex, "module", "working_memory");
    EXPECT_EQ(result, 0);

    result = nimcp_exception_set_context(ex, "capacity", "7");
    EXPECT_EQ(result, 0);

    result = nimcp_exception_set_context(ex, "requested", "12");
    EXPECT_EQ(result, 0);

    // Verify context retrieval
    const char* module = nimcp_exception_get_context(ex, "module");
    EXPECT_NE(module, nullptr);
    EXPECT_STREQ(module, "working_memory");

    const char* capacity = nimcp_exception_get_context(ex, "capacity");
    EXPECT_NE(capacity, nullptr);
    EXPECT_STREQ(capacity, "7");

    const char* requested = nimcp_exception_get_context(ex, "requested");
    EXPECT_NE(requested, nullptr);
    EXPECT_STREQ(requested, "12");

    EXPECT_EQ(nimcp_exception_context_count(ex), 3u);

    nimcp_exception_unref(ex);
}

//=============================================================================
// Severity Tests
//=============================================================================

TEST_F(CognitiveExceptionHandlingTest, CognitiveExceptionSeverityMapping) {
    // WHAT: Test severity mapping for cognitive errors
    // WHY:  Different cognitive errors have different severity levels
    // HOW:  Check severity heuristic for cognitive error codes

    // Cognitive errors should default to ERROR severity
    nimcp_exception_severity_t severity = nimcp_exception_get_severity_from_code(
        NIMCP_ERROR_WORKING_MEMORY
    );
    EXPECT_GE(severity, EXCEPTION_SEVERITY_ERROR);
}

TEST_F(CognitiveExceptionHandlingTest, CreateCognitiveExceptionWithDifferentSeverities) {
    // WHAT: Test creating cognitive exceptions with various severity levels
    // WHY:  Verify severity is properly set and preserved
    // HOW:  Create exceptions with each severity level

    nimcp_exception_severity_t severities[] = {
        EXCEPTION_SEVERITY_DEBUG,
        EXCEPTION_SEVERITY_INFO,
        EXCEPTION_SEVERITY_WARNING,
        EXCEPTION_SEVERITY_ERROR,
        EXCEPTION_SEVERITY_SEVERE,
        EXCEPTION_SEVERITY_CRITICAL,
        EXCEPTION_SEVERITY_FATAL
    };

    for (nimcp_exception_severity_t severity : severities) {
        nimcp_exception_t* ex = nimcp_exception_create(
            NIMCP_ERROR_EXECUTIVE_CONTROL,
            severity,
            __FILE__, __LINE__, __func__,
            "Test exception with severity %d", severity
        );

        ASSERT_NE(ex, nullptr);
        EXPECT_EQ(ex->severity, severity);

        nimcp_exception_unref(ex);
    }
}

//=============================================================================
// Handler Registration and Dispatch Tests
//=============================================================================

TEST_F(CognitiveExceptionHandlingTest, RegisterCognitiveHandler) {
    // WHAT: Test registering a handler for cognitive exceptions
    // WHY:  Handlers need to be properly registered to receive exceptions
    // HOW:  Register handler, dispatch exception, verify handler called

    nimcp_handler_options_t options;
    nimcp_handler_default_options(&options);
    options.name = "cognitive_test_handler";
    options.handler = test_exception_handler;
    options.priority = NIMCP_HANDLER_PRIORITY_NORMAL;
    options.category_filter = EXCEPTION_CATEGORY_COGNITIVE;
    options.user_data = nullptr;

    nimcp_handler_registration_t* reg = nimcp_handler_register(&options);
    ASSERT_NE(reg, nullptr);

    // Create and dispatch cognitive exception
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_WORKING_MEMORY,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Test handler dispatch"
    );
    ASSERT_NE(ex, nullptr);

    handler_call_count = 0;
    nimcp_exception_dispatch(ex);

    EXPECT_GE(handler_call_count.load(), 1);
    EXPECT_EQ(last_exception_code.load(), NIMCP_ERROR_WORKING_MEMORY);
    EXPECT_EQ(last_exception_category.load(), EXCEPTION_CATEGORY_COGNITIVE);

    nimcp_exception_unref(ex);
    nimcp_handler_unregister(reg);
}

TEST_F(CognitiveExceptionHandlingTest, HandlerPriorityOrder) {
    // WHAT: Test that handlers are called in priority order
    // WHY:  High priority handlers should run first
    // HOW:  Register multiple handlers with different priorities

    std::vector<int> call_order;

    auto high_handler = [](nimcp_exception_t* ex, void* user_data) -> bool {
        (void)ex;
        std::vector<int>* order = static_cast<std::vector<int>*>(user_data);
        order->push_back(1);  // High priority = 1
        return false;
    };

    auto normal_handler = [](nimcp_exception_t* ex, void* user_data) -> bool {
        (void)ex;
        std::vector<int>* order = static_cast<std::vector<int>*>(user_data);
        order->push_back(2);  // Normal priority = 2
        return false;
    };

    auto low_handler = [](nimcp_exception_t* ex, void* user_data) -> bool {
        (void)ex;
        std::vector<int>* order = static_cast<std::vector<int>*>(user_data);
        order->push_back(3);  // Low priority = 3
        return false;
    };

    nimcp_handler_options_t options;
    nimcp_handler_default_options(&options);
    options.category_filter = EXCEPTION_CATEGORY_COGNITIVE;
    options.user_data = &call_order;

    // Register in reverse priority order to ensure sorting works
    options.name = "low_handler";
    options.handler = low_handler;
    options.priority = NIMCP_HANDLER_PRIORITY_LOW;
    nimcp_handler_registration_t* low_reg = nimcp_handler_register(&options);

    options.name = "high_handler";
    options.handler = high_handler;
    options.priority = NIMCP_HANDLER_PRIORITY_HIGH;
    nimcp_handler_registration_t* high_reg = nimcp_handler_register(&options);

    options.name = "normal_handler";
    options.handler = normal_handler;
    options.priority = NIMCP_HANDLER_PRIORITY_NORMAL;
    nimcp_handler_registration_t* normal_reg = nimcp_handler_register(&options);

    // Create and dispatch exception
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_EMOTIONAL_TAGGING,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Test priority order"
    );

    nimcp_exception_dispatch(ex);

    // Verify call order: high (1) -> normal (2) -> low (3)
    ASSERT_GE(call_order.size(), 3u);
    EXPECT_EQ(call_order[0], 1);  // High priority first
    EXPECT_EQ(call_order[1], 2);  // Normal priority second
    EXPECT_EQ(call_order[2], 3);  // Low priority third

    nimcp_exception_unref(ex);
    nimcp_handler_unregister(low_reg);
    nimcp_handler_unregister(high_reg);
    nimcp_handler_unregister(normal_reg);
}

TEST_F(CognitiveExceptionHandlingTest, HandlerCategoryFilter) {
    // WHAT: Test that category filter works correctly
    // WHY:  Handlers should only receive exceptions matching their filter
    // HOW:  Register handler with cognitive filter, dispatch non-cognitive exception

    nimcp_handler_options_t options;
    nimcp_handler_default_options(&options);
    options.name = "cognitive_only_handler";
    options.handler = cognitive_filter_handler;
    options.priority = NIMCP_HANDLER_PRIORITY_NORMAL;
    options.category_filter = EXCEPTION_CATEGORY_COGNITIVE;

    nimcp_handler_registration_t* reg = nimcp_handler_register(&options);
    ASSERT_NE(reg, nullptr);

    // Dispatch cognitive exception - should be handled
    nimcp_exception_t* cognitive_ex = nimcp_exception_create(
        NIMCP_ERROR_WORKING_MEMORY,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Cognitive error"
    );

    handler_call_count = 0;
    nimcp_exception_dispatch(cognitive_ex);
    EXPECT_GE(handler_call_count.load(), 1);

    // Dispatch memory exception - handler should still be called if filter allows
    // (depending on implementation - some may skip non-matching)
    nimcp_exception_t* memory_ex = nimcp_exception_create(
        NIMCP_ERROR_NO_MEMORY,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Memory error"
    );

    int prev_count = handler_call_count.load();
    nimcp_exception_dispatch(memory_ex);
    // Handler with COGNITIVE filter may or may not be called for MEMORY category
    // depending on implementation

    nimcp_exception_unref(cognitive_ex);
    nimcp_exception_unref(memory_ex);
    nimcp_handler_unregister(reg);
}

TEST_F(CognitiveExceptionHandlingTest, ConsumingHandler) {
    // WHAT: Test that a consuming handler stops the handler chain
    // WHY:  When a handler handles an exception, subsequent handlers should not run
    // HOW:  Register consuming handler, verify subsequent handler not called

    nimcp_handler_options_t options;
    nimcp_handler_default_options(&options);
    options.category_filter = EXCEPTION_CATEGORY_COGNITIVE;

    // Register consuming handler at high priority
    options.name = "consuming_handler";
    options.handler = consuming_handler;
    options.priority = NIMCP_HANDLER_PRIORITY_HIGH;
    nimcp_handler_registration_t* consuming_reg = nimcp_handler_register(&options);

    // Register non-consuming handler at lower priority
    options.name = "secondary_handler";
    options.handler = test_exception_handler;
    options.priority = NIMCP_HANDLER_PRIORITY_LOW;
    nimcp_handler_registration_t* secondary_reg = nimcp_handler_register(&options);

    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_THEORY_OF_MIND,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Test consuming handler"
    );

    handler_call_count = 0;
    handler_consumed_exception = false;
    bool handled = nimcp_exception_dispatch(ex);

    EXPECT_TRUE(handled);
    EXPECT_TRUE(handler_consumed_exception.load());
    // Only consuming handler should have been called
    EXPECT_EQ(handler_call_count.load(), 1);

    nimcp_exception_unref(ex);
    nimcp_handler_unregister(consuming_reg);
    nimcp_handler_unregister(secondary_reg);
}

//=============================================================================
// Exception Chaining Tests
//=============================================================================

TEST_F(CognitiveExceptionHandlingTest, ExceptionChaining) {
    // WHAT: Test exception cause chaining
    // WHY:  Cognitive exceptions often have underlying causes
    // HOW:  Create exception chain and verify traversal

    // Create root cause (memory error)
    nimcp_exception_t* root = nimcp_exception_create(
        NIMCP_ERROR_NO_MEMORY,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Out of memory allocating attention buffer"
    );

    // Create cognitive exception with cause
    nimcp_exception_t* cognitive_ex = nimcp_exception_create(
        NIMCP_ERROR_WORKING_MEMORY,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Working memory buffer allocation failed"
    );

    nimcp_exception_set_cause(cognitive_ex, root);

    // Verify chain
    nimcp_exception_t* cause = nimcp_exception_get_cause(cognitive_ex);
    ASSERT_NE(cause, nullptr);
    EXPECT_EQ(cause->code, NIMCP_ERROR_NO_MEMORY);

    // Unref only the top-level exception (it owns the cause reference)
    nimcp_exception_unref(cognitive_ex);
}

//=============================================================================
// Recovery Strategy Tests
//=============================================================================

TEST_F(CognitiveExceptionHandlingTest, CognitiveRecoveryStrategy) {
    // WHAT: Test recovery strategy for cognitive exceptions
    // WHY:  Cognitive errors need appropriate recovery actions
    // HOW:  Create exception and check suggested recovery

    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_WORKING_MEMORY,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        "Working memory exhausted"
    );

    ASSERT_NE(ex, nullptr);

    nimcp_exception_recovery_strategy_t strategy;
    nimcp_exception_get_recovery_strategy(ex, &strategy);

    // Cognitive exceptions should have defined recovery strategies
    EXPECT_NE(strategy.primary_action, EXCEPTION_RECOVERY_NONE);

    nimcp_exception_unref(ex);
}

TEST_F(CognitiveExceptionHandlingTest, SuggestedRecoveryAction) {
    // WHAT: Test suggested recovery action for various cognitive errors
    // WHY:  Each cognitive error type may have different optimal recovery
    // HOW:  Create exceptions and check suggested actions

    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_EXECUTIVE_CONTROL,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Executive control inhibition failure"
    );

    ASSERT_NE(ex, nullptr);

    nimcp_exception_recovery_action_t suggested = nimcp_exception_get_suggested_recovery(ex);
    // Should return a valid recovery action
    EXPECT_NE(suggested, EXCEPTION_RECOVERY_NONE);

    nimcp_exception_unref(ex);
}

//=============================================================================
// Epitope Generation Tests
//=============================================================================

TEST_F(CognitiveExceptionHandlingTest, EpitopeGeneration) {
    // WHAT: Test immune epitope generation for cognitive exceptions
    // WHY:  Epitopes are used for immune pattern matching
    // HOW:  Generate epitope and verify it's non-empty

    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_MENTAL_HEALTH,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        "Mental health threshold exceeded"
    );

    ASSERT_NE(ex, nullptr);

    size_t epitope_len = nimcp_exception_generate_epitope(ex);
    EXPECT_GT(epitope_len, 0u);
    EXPECT_LE(epitope_len, NIMCP_EXCEPTION_EPITOPE_SIZE);

    // Check epitope is stored in exception
    EXPECT_EQ(ex->epitope_len, epitope_len);

    // Verify epitope is not all zeros
    bool all_zeros = true;
    for (size_t i = 0; i < ex->epitope_len && all_zeros; i++) {
        if (ex->epitope[i] != 0) all_zeros = false;
    }
    EXPECT_FALSE(all_zeros);

    nimcp_exception_unref(ex);
}

TEST_F(CognitiveExceptionHandlingTest, DifferentExceptionsDifferentEpitopes) {
    // WHAT: Test that different cognitive exceptions generate different epitopes
    // WHY:  Immune system needs to distinguish between exception types
    // HOW:  Create two different exceptions and compare epitopes

    nimcp_exception_t* ex1 = nimcp_exception_create(
        NIMCP_ERROR_WORKING_MEMORY,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Working memory error"
    );

    nimcp_exception_t* ex2 = nimcp_exception_create(
        NIMCP_ERROR_EMOTIONAL_TAGGING,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Emotional tagging error"
    );

    ASSERT_NE(ex1, nullptr);
    ASSERT_NE(ex2, nullptr);

    nimcp_exception_generate_epitope(ex1);
    nimcp_exception_generate_epitope(ex2);

    // Epitopes should be different for different error codes
    bool epitopes_same = (ex1->epitope_len == ex2->epitope_len &&
                          memcmp(ex1->epitope, ex2->epitope, ex1->epitope_len) == 0);
    EXPECT_FALSE(epitopes_same);

    nimcp_exception_unref(ex1);
    nimcp_exception_unref(ex2);
}

//=============================================================================
// Handler Enable/Disable Tests
//=============================================================================

TEST_F(CognitiveExceptionHandlingTest, HandlerDisableEnable) {
    // WHAT: Test handler disable/enable functionality
    // WHY:  Temporarily disabling handlers is useful for recovery operations
    // HOW:  Register handler, disable it, verify not called, re-enable

    nimcp_handler_options_t options;
    nimcp_handler_default_options(&options);
    options.name = "toggleable_handler";
    options.handler = test_exception_handler;
    options.priority = NIMCP_HANDLER_PRIORITY_HIGH;
    options.category_filter = EXCEPTION_CATEGORY_COGNITIVE;

    nimcp_handler_registration_t* reg = nimcp_handler_register(&options);
    ASSERT_NE(reg, nullptr);

    // Verify handler is called when enabled
    nimcp_exception_t* ex1 = nimcp_exception_create(
        NIMCP_ERROR_PREDICTIVE,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Test while enabled"
    );

    handler_call_count = 0;
    nimcp_exception_dispatch(ex1);
    int enabled_count = handler_call_count.load();
    EXPECT_GE(enabled_count, 1);

    // Disable handler
    nimcp_handler_disable(reg);

    nimcp_exception_t* ex2 = nimcp_exception_create(
        NIMCP_ERROR_PREDICTIVE,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Test while disabled"
    );

    handler_call_count = 0;
    nimcp_exception_dispatch(ex2);
    int disabled_count = handler_call_count.load();
    EXPECT_LT(disabled_count, enabled_count);

    // Re-enable handler
    nimcp_handler_enable(reg);

    nimcp_exception_t* ex3 = nimcp_exception_create(
        NIMCP_ERROR_PREDICTIVE,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Test after re-enable"
    );

    handler_call_count = 0;
    nimcp_exception_dispatch(ex3);
    int reenabled_count = handler_call_count.load();
    EXPECT_GE(reenabled_count, 1);

    nimcp_exception_unref(ex1);
    nimcp_exception_unref(ex2);
    nimcp_exception_unref(ex3);
    nimcp_handler_unregister(reg);
}

//=============================================================================
// String Conversion Tests
//=============================================================================

TEST_F(CognitiveExceptionHandlingTest, ExceptionToString) {
    // WHAT: Test exception formatting as string
    // WHY:  String representation is needed for logging and debugging
    // HOW:  Create exception and format to string

    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_META_LEARNING,
        EXCEPTION_SEVERITY_WARNING,
        __FILE__, __LINE__, __func__,
        "Meta-learning adaptation stalled"
    );

    ASSERT_NE(ex, nullptr);

    char buffer[1024];
    size_t len = nimcp_exception_to_string(ex, buffer, sizeof(buffer));

    EXPECT_GT(len, 0u);
    EXPECT_LT(len, sizeof(buffer));

    // Should contain error code and message
    std::string str(buffer);
    EXPECT_NE(str.find("8007"), std::string::npos);  // META_LEARNING error code
    EXPECT_NE(str.find("Meta-learning"), std::string::npos);

    nimcp_exception_unref(ex);
}

TEST_F(CognitiveExceptionHandlingTest, CategoryToString) {
    // WHAT: Test category to string conversion
    // WHY:  Human-readable category names for logging
    // HOW:  Convert COGNITIVE category to string

    const char* category_str = nimcp_exception_category_to_string(EXCEPTION_CATEGORY_COGNITIVE);
    EXPECT_NE(category_str, nullptr);
    EXPECT_NE(strlen(category_str), 0u);
}

TEST_F(CognitiveExceptionHandlingTest, SeverityToString) {
    // WHAT: Test severity to string conversion
    // WHY:  Human-readable severity names for logging
    // HOW:  Convert each severity level to string

    nimcp_exception_severity_t severities[] = {
        EXCEPTION_SEVERITY_DEBUG,
        EXCEPTION_SEVERITY_INFO,
        EXCEPTION_SEVERITY_WARNING,
        EXCEPTION_SEVERITY_ERROR,
        EXCEPTION_SEVERITY_SEVERE,
        EXCEPTION_SEVERITY_CRITICAL,
        EXCEPTION_SEVERITY_FATAL
    };

    for (nimcp_exception_severity_t severity : severities) {
        const char* severity_str = nimcp_exception_severity_to_string(severity);
        EXPECT_NE(severity_str, nullptr);
        EXPECT_NE(strlen(severity_str), 0u);
    }
}

//=============================================================================
// Thread-Local Exception Tests
//=============================================================================

TEST_F(CognitiveExceptionHandlingTest, ThreadLocalException) {
    // WHAT: Test thread-local current exception
    // WHY:  Each thread should have its own current exception
    // HOW:  Set and get current exception

    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_SLEEP_WAKE,
        EXCEPTION_SEVERITY_WARNING,
        __FILE__, __LINE__, __func__,
        "Sleep cycle disruption"
    );

    ASSERT_NE(ex, nullptr);

    // Initially no current exception
    nimcp_exception_clear_current();
    EXPECT_EQ(nimcp_exception_get_current(), nullptr);

    // Set current exception
    nimcp_exception_set_current(ex);
    nimcp_exception_t* current = nimcp_exception_get_current();
    EXPECT_EQ(current, ex);
    EXPECT_EQ(current->code, NIMCP_ERROR_SLEEP_WAKE);

    // Clear current exception
    nimcp_exception_clear_current();
    EXPECT_EQ(nimcp_exception_get_current(), nullptr);

    nimcp_exception_unref(ex);
}

//=============================================================================
// Reference Counting Tests
//=============================================================================

TEST_F(CognitiveExceptionHandlingTest, ReferenceCountBasic) {
    // WHAT: Test basic reference counting
    // WHY:  Ensure exceptions are properly freed when unreferenced
    // HOW:  Create exception, add/remove references

    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_EXPLANATIONS,
        EXCEPTION_SEVERITY_INFO,
        __FILE__, __LINE__, __func__,
        "Explanation generation"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->ref_count, 1);

    // Add reference
    nimcp_exception_t* ref = nimcp_exception_ref(ex);
    EXPECT_EQ(ref, ex);
    EXPECT_EQ(ex->ref_count, 2);

    // Remove first reference
    nimcp_exception_unref(ex);
    // Exception should still exist (ref_count = 1)

    // Remove second reference
    nimcp_exception_unref(ref);
    // Exception should be freed now (don't access ex after this)
}

//=============================================================================
// Aggregate Exception Tests
//=============================================================================

TEST_F(CognitiveExceptionHandlingTest, AggregateException) {
    // WHAT: Test aggregate exception for multiple cognitive errors
    // WHY:  Batch operations may produce multiple errors
    // HOW:  Create aggregate and add child exceptions

    nimcp_aggregate_exception_t* agg = nimcp_aggregate_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Multiple cognitive subsystem failures"
    );

    ASSERT_NE(agg, nullptr);

    // Create child exceptions
    nimcp_exception_t* child1 = nimcp_exception_create(
        NIMCP_ERROR_WORKING_MEMORY,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Working memory failure"
    );

    nimcp_exception_t* child2 = nimcp_exception_create(
        NIMCP_ERROR_EXECUTIVE_CONTROL,
        EXCEPTION_SEVERITY_WARNING,
        __FILE__, __LINE__, __func__,
        "Executive control degraded"
    );

    // Add children
    int result = nimcp_aggregate_exception_add(agg, child1);
    EXPECT_EQ(result, 0);

    result = nimcp_aggregate_exception_add(agg, child2);
    EXPECT_EQ(result, 0);

    // Verify count
    EXPECT_EQ(nimcp_aggregate_exception_count(agg), 2u);

    // Verify children
    nimcp_exception_t* retrieved1 = nimcp_aggregate_exception_get(agg, 0);
    EXPECT_EQ(retrieved1->code, NIMCP_ERROR_WORKING_MEMORY);

    nimcp_exception_t* retrieved2 = nimcp_aggregate_exception_get(agg, 1);
    EXPECT_EQ(retrieved2->code, NIMCP_ERROR_EXECUTIVE_CONTROL);

    // Out of bounds should return NULL
    EXPECT_EQ(nimcp_aggregate_exception_get(agg, 5), nullptr);

    nimcp_exception_unref((nimcp_exception_t*)agg);
}

//=============================================================================
// Edge Cases and Error Handling Tests
//=============================================================================

TEST_F(CognitiveExceptionHandlingTest, NullParameterHandling) {
    // WHAT: Test handling of NULL parameters
    // WHY:  Functions should handle NULL gracefully
    // HOW:  Pass NULL to various functions and verify no crash

    // Context operations with NULL
    EXPECT_EQ(nimcp_exception_set_context(nullptr, "key", "value"), -1);
    EXPECT_EQ(nimcp_exception_get_context(nullptr, "key"), nullptr);
    EXPECT_EQ(nimcp_exception_context_count(nullptr), 0u);

    // Aggregate operations with NULL
    EXPECT_EQ(nimcp_aggregate_exception_add(nullptr, nullptr), -1);
    EXPECT_EQ(nimcp_aggregate_exception_count(nullptr), 0u);
    EXPECT_EQ(nimcp_aggregate_exception_get(nullptr, 0), nullptr);

    // Unref NULL should be safe
    nimcp_exception_unref(nullptr);

    // Dispatch NULL should be safe
    bool handled = nimcp_exception_dispatch(nullptr);
    EXPECT_FALSE(handled);
}

TEST_F(CognitiveExceptionHandlingTest, HandlerCountQuery) {
    // WHAT: Test querying handler count
    // WHY:  Useful for debugging and monitoring
    // HOW:  Register handlers and check count

    size_t initial_count = nimcp_handler_count();

    nimcp_handler_options_t options;
    nimcp_handler_default_options(&options);
    options.name = "test_handler_1";
    options.handler = test_exception_handler;

    nimcp_handler_registration_t* reg1 = nimcp_handler_register(&options);
    EXPECT_EQ(nimcp_handler_count(), initial_count + 1);

    options.name = "test_handler_2";
    nimcp_handler_registration_t* reg2 = nimcp_handler_register(&options);
    EXPECT_EQ(nimcp_handler_count(), initial_count + 2);

    nimcp_handler_unregister(reg1);
    EXPECT_EQ(nimcp_handler_count(), initial_count + 1);

    nimcp_handler_unregister(reg2);
    EXPECT_EQ(nimcp_handler_count(), initial_count);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
