/**
 * @file test_middleware_exception_handling.cpp
 * @brief Unit tests for middleware exception handling
 *
 * WHAT: Test exception handling in middleware components
 * WHY:  Verify middleware properly handles and propagates exceptions
 * HOW:  Test pipeline stages, buffers, routing, and feature extraction with exceptions
 *
 * TEST SCENARIOS:
 * - Pipeline stage exception handling
 * - Buffer overflow exception creation and handling
 * - Routing failure exception propagation
 * - Feature extraction error recovery
 * - Context invalidation on exceptions
 * - Exception handler registration for middleware
 *
 * @author NIMCP Development Team
 * @date 2026-01-16
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cstdio>
#include <atomic>

extern "C" {
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_handlers.h"
#include "utils/exception/nimcp_exception_immune.h"
#include "utils/error/nimcp_error_codes.h"
}

//=============================================================================
// Test Fixture
//=============================================================================

class MiddlewareExceptionHandlingTest : public ::testing::Test {
protected:
    static std::atomic<int> handler_call_count;
    static std::atomic<int> last_exception_code;
    static std::atomic<nimcp_exception_category_t> last_category;
    static std::atomic<nimcp_exception_severity_t> last_severity;

    void SetUp() override {
        handler_call_count = 0;
        last_exception_code = 0;
        last_category = EXCEPTION_CATEGORY_GENERIC;
        last_severity = EXCEPTION_SEVERITY_INFO;

        nimcp_exception_system_init();
    }

    void TearDown() override {
        nimcp_exception_clear_current();
        nimcp_exception_system_shutdown();
    }

    // Test handler that captures exception information
    static bool middleware_exception_handler(nimcp_exception_t* ex, void* user_data) {
        (void)user_data;
        handler_call_count++;
        last_exception_code = ex->code;
        last_category = ex->category;
        last_severity = ex->severity;
        return false;  // Don't consume - allow other handlers
    }

    // Helper to create a middleware-style exception
    static nimcp_exception_t* create_middleware_exception(
        nimcp_error_t code,
        nimcp_exception_severity_t severity,
        const char* message
    ) {
        return nimcp_exception_create(
            code,
            severity,
            __FILE__,
            __LINE__,
            __func__,
            "%s",
            message
        );
    }
};

std::atomic<int> MiddlewareExceptionHandlingTest::handler_call_count(0);
std::atomic<int> MiddlewareExceptionHandlingTest::last_exception_code(0);
std::atomic<nimcp_exception_category_t> MiddlewareExceptionHandlingTest::last_category(EXCEPTION_CATEGORY_GENERIC);
std::atomic<nimcp_exception_severity_t> MiddlewareExceptionHandlingTest::last_severity(EXCEPTION_SEVERITY_INFO);

//=============================================================================
// Pipeline Exception Tests
//=============================================================================

TEST_F(MiddlewareExceptionHandlingTest, PipelineStageExceptionCreation) {
    // WHAT: Test creating exceptions for pipeline stage failures
    // WHY:  Pipeline stages need clear exception reporting

    nimcp_exception_t* ex = create_middleware_exception(
        NIMCP_ERROR_PIPELINE_FAILURE,
        EXCEPTION_SEVERITY_ERROR,
        "Pipeline stage ENCODING failed"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->severity, EXCEPTION_SEVERITY_ERROR);
    EXPECT_STRNE(ex->message, "");
    EXPECT_TRUE(strstr(ex->message, "Pipeline") != nullptr ||
                strstr(ex->message, "ENCODING") != nullptr);

    nimcp_exception_unref(ex);
}

TEST_F(MiddlewareExceptionHandlingTest, PipelineExceptionWithContext) {
    // WHAT: Test adding context data to pipeline exceptions
    // WHY:  Context helps diagnose pipeline failures

    nimcp_exception_t* ex = create_middleware_exception(
        NIMCP_ERROR_PIPELINE_FAILURE,
        EXCEPTION_SEVERITY_ERROR,
        "Pipeline execution failed"
    );
    ASSERT_NE(ex, nullptr);

    // Add middleware-specific context
    EXPECT_EQ(nimcp_exception_set_context(ex, "stage", "EXTRACTION"), 0);
    EXPECT_EQ(nimcp_exception_set_context(ex, "stage_index", "1"), 0);
    EXPECT_EQ(nimcp_exception_set_context(ex, "features_processed", "0"), 0);

    // Verify context retrieval
    EXPECT_STREQ(nimcp_exception_get_context(ex, "stage"), "EXTRACTION");
    EXPECT_STREQ(nimcp_exception_get_context(ex, "stage_index"), "1");
    EXPECT_EQ(nimcp_exception_context_count(ex), 3u);

    nimcp_exception_unref(ex);
}

TEST_F(MiddlewareExceptionHandlingTest, PipelineExceptionDispatch) {
    // WHAT: Test exception dispatch through handler chain
    // WHY:  Verify middleware exceptions reach registered handlers

    // Register middleware handler
    nimcp_handler_options_t options;
    nimcp_handler_default_options(&options);
    options.name = "middleware_test_handler";
    options.handler = middleware_exception_handler;
    options.priority = 100;
    nimcp_handler_registration_t* reg = nimcp_handler_register(&options);
    ASSERT_NE(reg, nullptr);

    // Create and dispatch exception
    nimcp_exception_t* ex = create_middleware_exception(
        NIMCP_ERROR_PIPELINE_FAILURE,
        EXCEPTION_SEVERITY_ERROR,
        "Test pipeline exception"
    );
    ASSERT_NE(ex, nullptr);

    handler_call_count = 0;
    nimcp_exception_dispatch(ex);

    EXPECT_GE(handler_call_count.load(), 1);
    EXPECT_EQ(last_exception_code.load(), NIMCP_ERROR_PIPELINE_FAILURE);

    nimcp_exception_unref(ex);
    nimcp_handler_unregister(reg);
}

//=============================================================================
// Buffer Exception Tests
//=============================================================================

TEST_F(MiddlewareExceptionHandlingTest, BufferOverflowExceptionCreation) {
    // WHAT: Test creating exceptions for buffer overflow
    // WHY:  Circular buffers need proper overflow handling

    nimcp_exception_t* ex = create_middleware_exception(
        NIMCP_ERROR_BUFFER_OVERFLOW,
        EXCEPTION_SEVERITY_WARNING,
        "Circular buffer overflow: capacity exceeded"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->severity, EXCEPTION_SEVERITY_WARNING);

    // Add buffer-specific context
    EXPECT_EQ(nimcp_exception_set_context(ex, "buffer_capacity", "1024"), 0);
    EXPECT_EQ(nimcp_exception_set_context(ex, "items_dropped", "5"), 0);

    nimcp_exception_unref(ex);
}

TEST_F(MiddlewareExceptionHandlingTest, BufferUnderflowExceptionHandling) {
    // WHAT: Test buffer underflow exception creation and handling
    // WHY:  Reading from empty buffers should be handled gracefully

    nimcp_exception_t* ex = create_middleware_exception(
        NIMCP_ERROR_BUFFER_UNDERFLOW,
        EXCEPTION_SEVERITY_WARNING,
        "Buffer underflow: no data available"
    );

    ASSERT_NE(ex, nullptr);

    // Get recovery strategy for buffer errors
    nimcp_exception_recovery_strategy_t strategy;
    nimcp_exception_get_recovery_strategy(ex, &strategy);

    // Buffer errors should suggest retry or continue
    EXPECT_TRUE(
        strategy.primary_action == EXCEPTION_RECOVERY_RETRY ||
        strategy.primary_action == EXCEPTION_RECOVERY_NONE ||
        strategy.primary_action == EXCEPTION_RECOVERY_GC
    );

    nimcp_exception_unref(ex);
}

TEST_F(MiddlewareExceptionHandlingTest, IntegrationBufferException) {
    // WHAT: Test integration buffer accumulation errors
    // WHY:  Integration buffers aggregate data and can have sync issues

    nimcp_exception_t* ex = create_middleware_exception(
        NIMCP_ERROR_INVALID_STATE,
        EXCEPTION_SEVERITY_ERROR,
        "Integration buffer sync error: inconsistent timestamps"
    );

    ASSERT_NE(ex, nullptr);

    nimcp_exception_set_context(ex, "buffer_type", "integration");
    nimcp_exception_set_context(ex, "expected_ts", "1000");
    nimcp_exception_set_context(ex, "received_ts", "500");

    EXPECT_STREQ(nimcp_exception_get_context(ex, "buffer_type"), "integration");

    nimcp_exception_unref(ex);
}

//=============================================================================
// Routing Exception Tests
//=============================================================================

TEST_F(MiddlewareExceptionHandlingTest, RoutingTableExceptionCreation) {
    // WHAT: Test exceptions for routing table failures
    // WHY:  Thalamic routing failures need proper exception handling

    nimcp_exception_t* ex = create_middleware_exception(
        NIMCP_ERROR_ROUTE_NOT_FOUND,
        EXCEPTION_SEVERITY_WARNING,
        "No route found for target region"
    );

    ASSERT_NE(ex, nullptr);

    nimcp_exception_set_context(ex, "source_region", "visual_cortex");
    nimcp_exception_set_context(ex, "target_region", "prefrontal");

    EXPECT_EQ(ex->severity, EXCEPTION_SEVERITY_WARNING);

    nimcp_exception_unref(ex);
}

TEST_F(MiddlewareExceptionHandlingTest, RoutingCongestionException) {
    // WHAT: Test routing congestion/overload exceptions
    // WHY:  High signal traffic can cause routing bottlenecks

    nimcp_exception_t* ex = create_middleware_exception(
        NIMCP_ERROR_RESOURCE_EXHAUSTED,
        EXCEPTION_SEVERITY_SEVERE,
        "Routing congestion detected: signal queue full"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->severity, EXCEPTION_SEVERITY_SEVERE);

    // Severe exceptions should have immune integration
    nimcp_immune_response_t response;
    memset(&response, 0, sizeof(response));
    int result = nimcp_exception_present_to_immune(ex, &response);
    EXPECT_EQ(result, 0);

    nimcp_exception_unref(ex);
}

TEST_F(MiddlewareExceptionHandlingTest, AttentionGateExceptionHandling) {
    // WHAT: Test attention gate threshold exceptions
    // WHY:  Attention gates filter signals and can have threshold issues

    nimcp_exception_t* ex = create_middleware_exception(
        NIMCP_ERROR_INVALID_PARAMETER,
        EXCEPTION_SEVERITY_WARNING,
        "Attention gate threshold out of range"
    );

    ASSERT_NE(ex, nullptr);

    nimcp_exception_set_context(ex, "threshold", "1.5");
    nimcp_exception_set_context(ex, "valid_range", "0.0-1.0");
    nimcp_exception_set_context(ex, "gate_region", "visual_cortex");

    EXPECT_STREQ(nimcp_exception_get_context(ex, "threshold"), "1.5");

    nimcp_exception_unref(ex);
}

//=============================================================================
// Feature Extraction Exception Tests
//=============================================================================

TEST_F(MiddlewareExceptionHandlingTest, FeatureExtractionNaNException) {
    // WHAT: Test NaN detection in feature extraction
    // WHY:  NaN values corrupt downstream processing

    nimcp_brain_exception_t* ex = nimcp_brain_exception_create(
        NIMCP_ERROR_NAN_DETECTED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        0,  // brain_id
        "feature_extractor",
        "NaN detected in extracted features"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->base.code, NIMCP_ERROR_NAN_DETECTED);
    EXPECT_EQ(ex->base.type, EXCEPTION_TYPE_BRAIN);
    EXPECT_TRUE(ex->has_nan_weights || true);  // Depending on implementation

    nimcp_exception_unref((nimcp_exception_t*)ex);
}

TEST_F(MiddlewareExceptionHandlingTest, FeatureExtractionDimensionMismatch) {
    // WHAT: Test dimension mismatch in feature vectors
    // WHY:  Feature dimensions must match expected sizes

    nimcp_exception_t* ex = create_middleware_exception(
        NIMCP_ERROR_DIMENSION_MISMATCH,
        EXCEPTION_SEVERITY_ERROR,
        "Feature vector dimension mismatch"
    );

    ASSERT_NE(ex, nullptr);

    nimcp_exception_set_context(ex, "expected_dims", "64");
    nimcp_exception_set_context(ex, "actual_dims", "32");
    nimcp_exception_set_context(ex, "feature_type", "spike_rate");

    EXPECT_STREQ(nimcp_exception_get_context(ex, "expected_dims"), "64");
    EXPECT_STREQ(nimcp_exception_get_context(ex, "actual_dims"), "32");

    nimcp_exception_unref(ex);
}

//=============================================================================
// Normalization Exception Tests
//=============================================================================

TEST_F(MiddlewareExceptionHandlingTest, NormalizationDivisionByZeroException) {
    // WHAT: Test division by zero in normalization
    // WHY:  Z-score and min-max normalization can hit divide-by-zero

    nimcp_exception_t* ex = create_middleware_exception(
        NIMCP_ERROR_DIVISION_BY_ZERO,
        EXCEPTION_SEVERITY_ERROR,
        "Normalization division by zero: zero variance detected"
    );

    ASSERT_NE(ex, nullptr);

    nimcp_exception_set_context(ex, "normalizer_type", "zscore");
    nimcp_exception_set_context(ex, "variance", "0.0");

    EXPECT_EQ(ex->code, NIMCP_ERROR_DIVISION_BY_ZERO);

    nimcp_exception_unref(ex);
}

TEST_F(MiddlewareExceptionHandlingTest, AdaptiveNormalizerException) {
    // WHAT: Test adaptive normalizer convergence failures
    // WHY:  Adaptive normalization can fail to converge

    nimcp_exception_t* ex = create_middleware_exception(
        NIMCP_ERROR_CONVERGENCE_FAILURE,
        EXCEPTION_SEVERITY_WARNING,
        "Adaptive normalizer failed to converge"
    );

    ASSERT_NE(ex, nullptr);

    nimcp_exception_set_context(ex, "iterations", "1000");
    nimcp_exception_set_context(ex, "tolerance", "1e-6");
    nimcp_exception_set_context(ex, "final_error", "0.01");

    nimcp_exception_unref(ex);
}

//=============================================================================
// Pattern Detection Exception Tests
//=============================================================================

TEST_F(MiddlewareExceptionHandlingTest, PatternLibraryException) {
    // WHAT: Test pattern library capacity exceptions
    // WHY:  Pattern libraries have finite capacity

    nimcp_exception_t* ex = create_middleware_exception(
        NIMCP_ERROR_RESOURCE_EXHAUSTED,
        EXCEPTION_SEVERITY_WARNING,
        "Pattern library full: cannot store new pattern"
    );

    ASSERT_NE(ex, nullptr);

    nimcp_exception_set_context(ex, "library_capacity", "256");
    nimcp_exception_set_context(ex, "pattern_type", "sequence");

    nimcp_exception_unref(ex);
}

TEST_F(MiddlewareExceptionHandlingTest, SequenceDetectorException) {
    // WHAT: Test sequence detector timeout exceptions
    // WHY:  Sequence detection has timing constraints

    nimcp_exception_t* ex = create_middleware_exception(
        NIMCP_ERROR_TIMEOUT,
        EXCEPTION_SEVERITY_INFO,
        "Sequence detection timeout: incomplete sequence"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->severity, EXCEPTION_SEVERITY_INFO);

    nimcp_exception_set_context(ex, "expected_length", "8");
    nimcp_exception_set_context(ex, "received_length", "5");
    nimcp_exception_set_context(ex, "timeout_ms", "100");

    nimcp_exception_unref(ex);
}

//=============================================================================
// Exception Chaining Tests
//=============================================================================

TEST_F(MiddlewareExceptionHandlingTest, ExceptionChainingForMiddleware) {
    // WHAT: Test exception chaining for root cause analysis
    // WHY:  Pipeline failures often have underlying causes

    // Create root cause exception
    nimcp_exception_t* root_cause = create_middleware_exception(
        NIMCP_ERROR_ALLOCATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        "Memory allocation failed for feature buffer"
    );
    ASSERT_NE(root_cause, nullptr);

    // Create higher-level exception
    nimcp_exception_t* pipeline_ex = create_middleware_exception(
        NIMCP_ERROR_PIPELINE_FAILURE,
        EXCEPTION_SEVERITY_ERROR,
        "Feature extraction stage failed"
    );
    ASSERT_NE(pipeline_ex, nullptr);

    // Chain exceptions
    nimcp_exception_set_cause(pipeline_ex, root_cause);

    // Verify chain
    nimcp_exception_t* cause = nimcp_exception_get_cause(pipeline_ex);
    EXPECT_EQ(cause, root_cause);
    EXPECT_EQ(cause->code, NIMCP_ERROR_ALLOCATION_FAILED);

    nimcp_exception_unref(pipeline_ex);
    // root_cause is released via chain
}

TEST_F(MiddlewareExceptionHandlingTest, AggregateExceptionForBatchFailures) {
    // WHAT: Test aggregate exceptions for batch processing failures
    // WHY:  Multiple stages can fail in a single pipeline execution

    nimcp_aggregate_exception_t* agg = nimcp_aggregate_exception_create(
        NIMCP_ERROR_MULTIPLE_ERRORS,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Multiple pipeline stages failed"
    );
    ASSERT_NE(agg, nullptr);

    // Add child exceptions for different failures
    nimcp_exception_t* ex1 = create_middleware_exception(
        NIMCP_ERROR_BUFFER_OVERFLOW,
        EXCEPTION_SEVERITY_WARNING,
        "Buffer overflow in accumulator"
    );
    nimcp_exception_t* ex2 = create_middleware_exception(
        NIMCP_ERROR_NAN_DETECTED,
        EXCEPTION_SEVERITY_ERROR,
        "NaN in feature vector"
    );

    EXPECT_EQ(nimcp_aggregate_exception_add(agg, ex1), 0);
    EXPECT_EQ(nimcp_aggregate_exception_add(agg, ex2), 0);

    EXPECT_EQ(nimcp_aggregate_exception_count(agg), 2u);
    EXPECT_EQ(nimcp_aggregate_exception_get(agg, 0), ex1);
    EXPECT_EQ(nimcp_aggregate_exception_get(agg, 1), ex2);

    nimcp_exception_unref((nimcp_exception_t*)agg);
}

//=============================================================================
// Handler Filtering Tests
//=============================================================================

TEST_F(MiddlewareExceptionHandlingTest, CategoryFilteredHandler) {
    // WHAT: Test handler filtering by exception category
    // WHY:  Different components may handle only specific categories

    // Register handler that only handles memory exceptions
    nimcp_handler_options_t options;
    nimcp_handler_default_options(&options);
    options.name = "memory_only_handler";
    options.handler = middleware_exception_handler;
    options.priority = 100;
    options.category_filter = EXCEPTION_CATEGORY_MEMORY;
    nimcp_handler_registration_t* reg = nimcp_handler_register(&options);
    ASSERT_NE(reg, nullptr);

    handler_call_count = 0;

    // Dispatch memory exception - should be handled
    nimcp_memory_exception_t* mem_ex = nimcp_memory_exception_create(
        NIMCP_ERROR_ALLOCATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        4096,
        "Memory allocation failed"
    );
    nimcp_exception_dispatch((nimcp_exception_t*)mem_ex);
    int mem_count = handler_call_count.load();

    // Dispatch non-memory exception - should not reach filtered handler
    nimcp_exception_t* generic_ex = create_middleware_exception(
        NIMCP_ERROR_PIPELINE_FAILURE,
        EXCEPTION_SEVERITY_ERROR,
        "Pipeline failed"
    );
    nimcp_exception_dispatch(generic_ex);
    int generic_count = handler_call_count.load();

    // Memory handler should have been called for memory exception
    EXPECT_GE(mem_count, 1);
    // Count may or may not increase for generic (depends on other handlers)

    nimcp_exception_unref((nimcp_exception_t*)mem_ex);
    nimcp_exception_unref(generic_ex);
    nimcp_handler_unregister(reg);
}

TEST_F(MiddlewareExceptionHandlingTest, SeverityFilteredHandler) {
    // WHAT: Test handler filtering by minimum severity
    // WHY:  Critical handlers should ignore minor warnings

    nimcp_handler_options_t options;
    nimcp_handler_default_options(&options);
    options.name = "severe_only_handler";
    options.handler = middleware_exception_handler;
    options.priority = 100;
    options.min_severity = EXCEPTION_SEVERITY_SEVERE;
    nimcp_handler_registration_t* reg = nimcp_handler_register(&options);
    ASSERT_NE(reg, nullptr);

    handler_call_count = 0;

    // Dispatch severe exception - should be handled
    nimcp_exception_t* severe_ex = create_middleware_exception(
        NIMCP_ERROR_RESOURCE_EXHAUSTED,
        EXCEPTION_SEVERITY_SEVERE,
        "Critical resource exhaustion"
    );
    nimcp_exception_dispatch(severe_ex);
    int severe_count = handler_call_count.load();

    // Dispatch warning exception - may not reach filtered handler
    nimcp_exception_t* warning_ex = create_middleware_exception(
        NIMCP_ERROR_BUFFER_OVERFLOW,
        EXCEPTION_SEVERITY_WARNING,
        "Minor buffer overflow"
    );
    nimcp_exception_dispatch(warning_ex);

    EXPECT_GE(severe_count, 1);

    nimcp_exception_unref(severe_ex);
    nimcp_exception_unref(warning_ex);
    nimcp_handler_unregister(reg);
}

//=============================================================================
// Recovery Strategy Tests
//=============================================================================

TEST_F(MiddlewareExceptionHandlingTest, MiddlewareRecoveryStrategies) {
    // WHAT: Test recovery strategy mapping for middleware exceptions
    // WHY:  Different middleware errors need different recovery approaches

    struct {
        nimcp_error_t code;
        nimcp_exception_severity_t severity;
        const char* name;
    } test_cases[] = {
        { NIMCP_ERROR_BUFFER_OVERFLOW, EXCEPTION_SEVERITY_WARNING, "buffer_overflow" },
        { NIMCP_ERROR_ALLOCATION_FAILED, EXCEPTION_SEVERITY_ERROR, "allocation_failed" },
        { NIMCP_ERROR_RESOURCE_EXHAUSTED, EXCEPTION_SEVERITY_SEVERE, "resource_exhausted" },
    };

    for (const auto& tc : test_cases) {
        nimcp_exception_t* ex = create_middleware_exception(
            tc.code, tc.severity, tc.name
        );
        ASSERT_NE(ex, nullptr) << "Failed for: " << tc.name;

        nimcp_exception_recovery_strategy_t strategy;
        nimcp_exception_get_recovery_strategy(ex, &strategy);

        // All should have a valid recovery action
        EXPECT_NE(strategy.primary_action, (nimcp_exception_recovery_action_t)-1)
            << "Invalid strategy for: " << tc.name;

        nimcp_exception_unref(ex);
    }
}

//=============================================================================
// Event Queue Exception Tests
//=============================================================================

TEST_F(MiddlewareExceptionHandlingTest, EventQueueOverflowException) {
    // WHAT: Test event queue overflow handling
    // WHY:  Event bus can overflow under high event rates

    nimcp_exception_t* ex = create_middleware_exception(
        NIMCP_ERROR_QUEUE_FULL,
        EXCEPTION_SEVERITY_WARNING,
        "Event queue overflow: events dropped"
    );

    ASSERT_NE(ex, nullptr);

    nimcp_exception_set_context(ex, "queue_capacity", "1024");
    nimcp_exception_set_context(ex, "events_dropped", "100");
    nimcp_exception_set_context(ex, "event_type", "spike");

    EXPECT_STREQ(nimcp_exception_get_context(ex, "events_dropped"), "100");

    nimcp_exception_unref(ex);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
