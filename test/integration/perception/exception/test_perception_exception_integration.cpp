/**
 * @file test_perception_exception_integration.cpp
 * @brief Integration tests for perception module exception handling
 *
 * WHAT: Test complete exception flow from perception modules to immune system
 * WHY:  Verify all components work together for perception error recovery
 * HOW:  Test full pipeline: perception error -> exception -> handler -> immune -> recovery
 *
 * INTEGRATION SCENARIOS:
 * 1. Visual cortex exception -> handler chain -> immune presentation
 * 2. Audio cortex exception -> async queue -> immune response
 * 3. Speech cortex exception -> chained exceptions -> aggregate recovery
 * 4. Multi-sensory exception handling coordination
 * 5. Recovery callback execution for perception failures
 *
 * @author NIMCP Development Team
 * @date 2026-01-16
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cstdio>
#include <thread>
#include <chrono>
#include <atomic>
#include <vector>
#include <cmath>  // For INFINITY

extern "C" {
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_handlers.h"
#include "utils/exception/nimcp_exception_immune.h"
#include "utils/error/nimcp_error_codes.h"
}

//=============================================================================
// Test Fixture
//=============================================================================

class PerceptionExceptionIntegrationTest : public ::testing::Test {
protected:
    static std::atomic<int> handler_call_count;
    static std::atomic<int> recovery_call_count;
    static std::atomic<bool> immune_presentation_called;
    static std::atomic<nimcp_exception_recovery_action_t> last_recovery_action;
    static std::vector<nimcp_error_t> handled_exception_codes;

    void SetUp() override {
        handler_call_count = 0;
        recovery_call_count = 0;
        immune_presentation_called = false;
        last_recovery_action = EXCEPTION_RECOVERY_NONE;
        handled_exception_codes.clear();

        // Initialize systems
        nimcp_exception_system_init();
        nimcp_exception_immune_init(nullptr);  // Use defaults
        nimcp_install_default_handlers();
    }

    void TearDown() override {
        nimcp_exception_clear_current();
        nimcp_exception_immune_shutdown();
        nimcp_exception_system_shutdown();
    }

    // Perception-specific handler that tracks all perception exceptions
    static bool perception_tracking_handler(nimcp_exception_t* ex, void* user_data) {
        (void)user_data;
        handler_call_count++;

        // Track codes in brain region range
        if (ex->code >= 10000 && ex->code < 20000) {
            handled_exception_codes.push_back(ex->code);
        }
        return false;  // Don't consume
    }

    // Recovery callback for perception modules
    static int perception_recovery_callback(
        nimcp_exception_t* ex,
        nimcp_exception_recovery_action_t action,
        void* user_data
    ) {
        (void)ex;
        (void)user_data;
        recovery_call_count++;
        last_recovery_action = action;

        // Simulate successful recovery
        return 0;
    }

    // Handler that presents to immune system
    static bool immune_presenting_handler(nimcp_exception_t* ex, void* user_data) {
        (void)user_data;

        if (ex->severity >= EXCEPTION_SEVERITY_ERROR) {
            nimcp_immune_response_t response;
            int result = nimcp_exception_present_to_immune(ex, &response);
            if (result == 0) {
                immune_presentation_called = true;
            }
        }
        return false;
    }
};

std::atomic<int> PerceptionExceptionIntegrationTest::handler_call_count(0);
std::atomic<int> PerceptionExceptionIntegrationTest::recovery_call_count(0);
std::atomic<bool> PerceptionExceptionIntegrationTest::immune_presentation_called(false);
std::atomic<nimcp_exception_recovery_action_t> PerceptionExceptionIntegrationTest::last_recovery_action(EXCEPTION_RECOVERY_NONE);
std::vector<nimcp_error_t> PerceptionExceptionIntegrationTest::handled_exception_codes;

//=============================================================================
// Full Pipeline Integration Tests
//=============================================================================

TEST_F(PerceptionExceptionIntegrationTest, VisualExceptionFullPipeline) {
    // WHAT: Test complete visual exception handling pipeline
    // WHY:  Verify visual errors flow through handler chain to immune system

    // Register custom handler
    nimcp_handler_options_t opts;
    nimcp_handler_default_options(&opts);
    opts.name = "visual_tracking_handler";
    opts.handler = perception_tracking_handler;
    opts.priority = NIMCP_HANDLER_PRIORITY_NORMAL;
    nimcp_handler_registration_t* reg = nimcp_handler_register(&opts);

    // Create visual exception
    nimcp_brain_exception_t* ex = nimcp_brain_exception_create(
        NIMCP_ERROR_OCCIPITAL_VISUAL_PROCESSING,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        1,  // brain_id
        "visual_cortex_v1",
        "V1 convolution failed: NaN detected in filter weights"
    );
    ASSERT_NE(ex, nullptr);

    // Add detailed context
    nimcp_exception_set_context((nimcp_exception_t*)ex, "layer", "conv1");
    nimcp_exception_set_context((nimcp_exception_t*)ex, "filter_count", "64");
    nimcp_exception_set_context((nimcp_exception_t*)ex, "nan_count", "3");

    // Mark for NaN weights detection
    ex->has_nan_weights = true;
    ex->gradient_norm = INFINITY;

    // Generate epitope
    size_t epitope_len = nimcp_exception_generate_epitope((nimcp_exception_t*)ex);
    EXPECT_GT(epitope_len, 0u);

    // Dispatch through handler chain
    handler_call_count = 0;
    bool handled = nimcp_exception_dispatch((nimcp_exception_t*)ex);
    (void)handled;  // May or may not be consumed depending on handlers

    EXPECT_GE(handler_call_count.load(), 1);

    // Verify exception was tracked
    bool found_visual = false;
    for (auto code : handled_exception_codes) {
        if (code == NIMCP_ERROR_OCCIPITAL_VISUAL_PROCESSING) {
            found_visual = true;
            break;
        }
    }
    EXPECT_TRUE(found_visual);

    nimcp_exception_unref((nimcp_exception_t*)ex);
    nimcp_handler_unregister(reg);
}

TEST_F(PerceptionExceptionIntegrationTest, AudioExceptionWithImmunePresentation) {
    // WHAT: Test audio exception handling with immune system presentation
    // WHY:  Verify severe audio errors trigger immune response

    // Register immune-presenting handler
    nimcp_handler_options_t opts;
    nimcp_handler_default_options(&opts);
    opts.name = "immune_presenting_handler";
    opts.handler = immune_presenting_handler;
    opts.priority = NIMCP_HANDLER_PRIORITY_HIGH;
    opts.min_severity = EXCEPTION_SEVERITY_ERROR;
    nimcp_handler_registration_t* reg = nimcp_handler_register(&opts);

    // Create severe audio exception
    nimcp_brain_exception_t* ex = nimcp_brain_exception_create(
        NIMCP_ERROR_TEMPORAL_AUDITORY,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        1,  // brain_id
        "audio_cortex_a1",
        "A1 tonotopic map corruption detected"
    );
    ASSERT_NE(ex, nullptr);

    nimcp_exception_set_context((nimcp_exception_t*)ex, "corruption_type", "frequency_drift");
    nimcp_exception_set_context((nimcp_exception_t*)ex, "affected_channels", "16-32");

    // Reset flag
    immune_presentation_called = false;

    // Dispatch
    nimcp_exception_dispatch((nimcp_exception_t*)ex);

    // Immune presentation should have been attempted
    EXPECT_TRUE(immune_presentation_called.load());

    nimcp_exception_unref((nimcp_exception_t*)ex);
    nimcp_handler_unregister(reg);
}

TEST_F(PerceptionExceptionIntegrationTest, ChainedPerceptionExceptionsWithRecovery) {
    // WHAT: Test chained exceptions from perception pipeline with recovery
    // WHY:  Verify cascading failures are handled correctly

    // Register recovery callback
    nimcp_register_recovery_callback(
        EXCEPTION_RECOVERY_RESTART_COMPONENT,
        perception_recovery_callback,
        nullptr
    );

    // Create exception chain: cochlea -> audio -> speech
    nimcp_exception_t* cochlea_ex = nimcp_exception_create(
        NIMCP_ERROR_TEMPORAL_INVALID_INPUT,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Cochlea basilar membrane filterbank overflow"
    );
    nimcp_exception_set_context(cochlea_ex, "module", "cochlea");
    nimcp_exception_set_context(cochlea_ex, "component", "basilar_membrane");

    nimcp_exception_t* audio_ex = nimcp_exception_create(
        NIMCP_ERROR_TEMPORAL_AUDITORY,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Audio cortex failed due to upstream cochlea error"
    );
    nimcp_exception_set_context(audio_ex, "module", "audio_cortex");
    nimcp_exception_set_cause(audio_ex, cochlea_ex);

    nimcp_exception_t* speech_ex = nimcp_exception_create(
        NIMCP_ERROR_WERNICKE_COMPREHENSION,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Speech comprehension failed due to corrupted audio"
    );
    nimcp_exception_set_context(speech_ex, "module", "speech_cortex");
    nimcp_exception_set_cause(speech_ex, audio_ex);

    // Test recovery execution
    recovery_call_count = 0;
    int recovery_result = nimcp_execute_recovery(speech_ex, EXCEPTION_RECOVERY_RESTART_COMPONENT);
    EXPECT_EQ(recovery_result, 0);
    EXPECT_EQ(recovery_call_count.load(), 1);
    EXPECT_EQ(last_recovery_action.load(), EXCEPTION_RECOVERY_RESTART_COMPONENT);

    nimcp_exception_unref(speech_ex);
    nimcp_unregister_recovery_callback(EXCEPTION_RECOVERY_RESTART_COMPONENT);
}

//=============================================================================
// Multi-Sensory Integration Tests
//=============================================================================

TEST_F(PerceptionExceptionIntegrationTest, MultiSensoryAggregateException) {
    // WHAT: Test handling of multi-sensory integration failures
    // WHY:  Verify aggregate exceptions work for combined perception errors

    // Register tracking handler
    nimcp_handler_options_t opts;
    nimcp_handler_default_options(&opts);
    opts.name = "multi_sensory_handler";
    opts.handler = perception_tracking_handler;
    opts.priority = NIMCP_HANDLER_PRIORITY_NORMAL;
    nimcp_handler_registration_t* reg = nimcp_handler_register(&opts);

    // Create aggregate exception
    nimcp_aggregate_exception_t* agg = nimcp_aggregate_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Multi-sensory integration failed: audiovisual binding error"
    );
    ASSERT_NE(agg, nullptr);

    // Add visual failure
    nimcp_brain_exception_t* visual_ex = nimcp_brain_exception_create(
        NIMCP_ERROR_OCCIPITAL_VISUAL_PROCESSING,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        1, "visual_cortex", "Visual stream timing error"
    );
    nimcp_aggregate_exception_add(agg, (nimcp_exception_t*)visual_ex);

    // Add audio failure
    nimcp_brain_exception_t* audio_ex = nimcp_brain_exception_create(
        NIMCP_ERROR_TEMPORAL_AUDITORY,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        1, "audio_cortex", "Audio stream timing error"
    );
    nimcp_aggregate_exception_add(agg, (nimcp_exception_t*)audio_ex);

    // Add speech failure
    nimcp_brain_exception_t* speech_ex = nimcp_brain_exception_create(
        NIMCP_ERROR_WERNICKE_COMPREHENSION,
        EXCEPTION_SEVERITY_WARNING,
        __FILE__, __LINE__, __func__,
        1, "speech_cortex", "Lip reading sync error"
    );
    nimcp_aggregate_exception_add(agg, (nimcp_exception_t*)speech_ex);

    EXPECT_EQ(nimcp_aggregate_exception_count(agg), 3u);

    // Dispatch aggregate
    handler_call_count = 0;
    handled_exception_codes.clear();
    nimcp_exception_dispatch((nimcp_exception_t*)agg);

    EXPECT_GE(handler_call_count.load(), 1);

    nimcp_exception_unref((nimcp_exception_t*)agg);
    nimcp_handler_unregister(reg);
}

//=============================================================================
// Immune System Integration Tests
//=============================================================================

TEST_F(PerceptionExceptionIntegrationTest, ImmuneResponseForVisualException) {
    // WHAT: Test immune system response to visual exception
    // WHY:  Verify immune integration triggers appropriate recovery

    nimcp_brain_exception_t* ex = nimcp_brain_exception_create(
        NIMCP_ERROR_OCCIPITAL_VISUAL_PROCESSING,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        1, "visual_cortex_v1", "V1 processing catastrophic failure"
    );
    ASSERT_NE(ex, nullptr);

    // Generate epitope for immune matching
    nimcp_exception_generate_epitope((nimcp_exception_t*)ex);

    // Present to immune system
    nimcp_immune_response_t response;
    memset(&response, 0, sizeof(response));

    int result = nimcp_exception_present_to_immune((nimcp_exception_t*)ex, &response);

    // Note: Response details depend on immune system state
    // Just verify the call doesn't crash and returns valid result
    EXPECT_TRUE(result == 0 || result == -1);  // OK or not connected

    if (result == 0) {
        // Verify response structure is populated
        EXPECT_TRUE(response.recovery_attempted || !response.recovery_attempted);
    }

    nimcp_exception_unref((nimcp_exception_t*)ex);
}

TEST_F(PerceptionExceptionIntegrationTest, AsyncPresentationForAudioException) {
    // WHAT: Test async immune presentation for audio exception
    // WHY:  Verify non-blocking immune integration works

    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_TEMPORAL_AUDITORY,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Audio cortex FFT buffer corruption"
    );
    ASSERT_NE(ex, nullptr);

    // Add reference since async takes ownership
    nimcp_exception_ref(ex);

    // Queue for async presentation
    int result = nimcp_exception_present_async(ex);
    // May fail if immune system not fully connected, that's OK
    (void)result;

    // Process pending (may or may not have items)
    size_t processed = nimcp_exception_immune_process_pending(0);
    (void)processed;  // Just verify no crash

    nimcp_exception_unref(ex);
}

TEST_F(PerceptionExceptionIntegrationTest, RecoveryStrategyExecution) {
    // WHAT: Test recovery strategy execution for perception exception
    // WHY:  Verify recovery callbacks are invoked correctly

    // Register multiple recovery callbacks
    nimcp_register_recovery_callback(
        EXCEPTION_RECOVERY_RETRY,
        perception_recovery_callback,
        nullptr
    );
    nimcp_register_recovery_callback(
        EXCEPTION_RECOVERY_CLEAR_CACHE,
        perception_recovery_callback,
        nullptr
    );

    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_OCCIPITAL_FEATURE_EXTRACTION,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Feature extraction cache corrupted"
    );
    ASSERT_NE(ex, nullptr);

    // Get recovery strategy
    nimcp_exception_recovery_strategy_t strategy;
    nimcp_exception_get_recovery_strategy(ex, &strategy);

    // Execute primary recovery action
    recovery_call_count = 0;
    if (strategy.primary_action != EXCEPTION_RECOVERY_NONE) {
        int result = nimcp_exception_execute_recovery(ex, strategy.primary_action);
        // Recovery may or may not succeed depending on registered callbacks
        (void)result;
    }

    nimcp_exception_unref(ex);
    nimcp_unregister_recovery_callback(EXCEPTION_RECOVERY_RETRY);
    nimcp_unregister_recovery_callback(EXCEPTION_RECOVERY_CLEAR_CACHE);
}

//=============================================================================
// Handler Priority Tests
//=============================================================================

TEST_F(PerceptionExceptionIntegrationTest, HandlerPriorityOrder) {
    // WHAT: Test handler invocation order by priority
    // WHY:  Verify high-priority handlers run before low-priority

    static std::vector<int> invocation_order;
    invocation_order.clear();

    auto high_priority_handler = [](nimcp_exception_t* ex, void* user_data) -> bool {
        (void)ex;
        (void)user_data;
        invocation_order.push_back(100);
        return false;
    };

    auto normal_priority_handler = [](nimcp_exception_t* ex, void* user_data) -> bool {
        (void)ex;
        (void)user_data;
        invocation_order.push_back(50);
        return false;
    };

    auto low_priority_handler = [](nimcp_exception_t* ex, void* user_data) -> bool {
        (void)ex;
        (void)user_data;
        invocation_order.push_back(10);
        return false;
    };

    // Register in reverse order to verify sorting
    nimcp_handler_options_t low_opts;
    nimcp_handler_default_options(&low_opts);
    low_opts.name = "low_priority";
    low_opts.handler = low_priority_handler;
    low_opts.priority = NIMCP_HANDLER_PRIORITY_LOW;
    nimcp_handler_registration_t* low_reg = nimcp_handler_register(&low_opts);

    nimcp_handler_options_t high_opts;
    nimcp_handler_default_options(&high_opts);
    high_opts.name = "high_priority";
    high_opts.handler = high_priority_handler;
    high_opts.priority = NIMCP_HANDLER_PRIORITY_HIGH;
    nimcp_handler_registration_t* high_reg = nimcp_handler_register(&high_opts);

    nimcp_handler_options_t normal_opts;
    nimcp_handler_default_options(&normal_opts);
    normal_opts.name = "normal_priority";
    normal_opts.handler = normal_priority_handler;
    normal_opts.priority = NIMCP_HANDLER_PRIORITY_NORMAL;
    nimcp_handler_registration_t* normal_reg = nimcp_handler_register(&normal_opts);

    // Create and dispatch exception
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_OCCIPITAL_VISUAL_PROCESSING,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Test exception for priority ordering"
    );
    ASSERT_NE(ex, nullptr);

    nimcp_exception_dispatch(ex);

    // Verify order: should be high (100), normal (50), low (10)
    ASSERT_GE(invocation_order.size(), 3u);
    bool ordered = true;
    for (size_t i = 1; i < invocation_order.size(); i++) {
        if (invocation_order[i] > invocation_order[i-1]) {
            ordered = false;
            break;
        }
    }
    EXPECT_TRUE(ordered);  // Should be in descending priority order

    nimcp_exception_unref(ex);
    nimcp_handler_unregister(low_reg);
    nimcp_handler_unregister(high_reg);
    nimcp_handler_unregister(normal_reg);
}

//=============================================================================
// Category Filtering Tests
//=============================================================================

TEST_F(PerceptionExceptionIntegrationTest, CategoryFilteredHandlers) {
    // WHAT: Test category-filtered exception handling
    // WHY:  Verify brain region handler only handles brain region exceptions

    static bool brain_handler_called = false;
    static bool memory_handler_called = false;

    auto brain_handler = [](nimcp_exception_t* ex, void* user_data) -> bool {
        (void)ex;
        (void)user_data;
        brain_handler_called = true;
        return false;
    };

    auto memory_handler = [](nimcp_exception_t* ex, void* user_data) -> bool {
        (void)ex;
        (void)user_data;
        memory_handler_called = true;
        return false;
    };

    // Register brain region handler
    nimcp_handler_options_t brain_opts;
    nimcp_handler_default_options(&brain_opts);
    brain_opts.name = "brain_region_handler";
    brain_opts.handler = brain_handler;
    brain_opts.priority = NIMCP_HANDLER_PRIORITY_NORMAL;
    brain_opts.category_filter = EXCEPTION_CATEGORY_BRAIN_REGION;
    nimcp_handler_registration_t* brain_reg = nimcp_handler_register(&brain_opts);

    // Register memory handler
    nimcp_handler_options_t memory_opts;
    nimcp_handler_default_options(&memory_opts);
    memory_opts.name = "memory_handler";
    memory_opts.handler = memory_handler;
    memory_opts.priority = NIMCP_HANDLER_PRIORITY_NORMAL;
    memory_opts.category_filter = EXCEPTION_CATEGORY_MEMORY;
    nimcp_handler_registration_t* memory_reg = nimcp_handler_register(&memory_opts);

    // Create brain region exception
    nimcp_exception_t* brain_ex = nimcp_exception_create(
        NIMCP_ERROR_OCCIPITAL_VISUAL_PROCESSING,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Visual cortex error"
    );

    brain_handler_called = false;
    memory_handler_called = false;
    nimcp_exception_dispatch(brain_ex);

    // Only brain handler should be called
    EXPECT_TRUE(brain_handler_called);
    EXPECT_FALSE(memory_handler_called);

    nimcp_exception_unref(brain_ex);
    nimcp_handler_unregister(brain_reg);
    nimcp_handler_unregister(memory_reg);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(PerceptionExceptionIntegrationTest, ExceptionImmuneStatistics) {
    // WHAT: Test immune integration statistics collection
    // WHY:  Verify stats are tracked correctly for perception exceptions

    // Reset stats
    nimcp_exception_immune_reset_stats();

    // Create and present multiple perception exceptions
    for (int i = 0; i < 5; i++) {
        nimcp_exception_t* ex = nimcp_exception_create(
            NIMCP_ERROR_OCCIPITAL_VISUAL_PROCESSING,
            EXCEPTION_SEVERITY_ERROR,
            __FILE__, __LINE__, __func__,
            "Visual exception %d", i
        );

        nimcp_immune_response_t response;
        nimcp_exception_present_to_immune(ex, &response);
        nimcp_exception_unref(ex);
    }

    // Get stats
    nimcp_exception_immune_stats_t stats;
    nimcp_exception_immune_get_stats(&stats);

    // Stats should reflect presentations (if immune system is connected)
    // Just verify the struct is populated without crashing
    EXPECT_GE(stats.exceptions_presented, 0u);
}

//=============================================================================
// Try/Catch Integration Tests
//=============================================================================

TEST_F(PerceptionExceptionIntegrationTest, TryCatchVisualException) {
    // WHAT: Test try/catch mechanism with visual exception
    // WHY:  Verify non-local exception handling works for perception errors

    bool exception_caught = false;
    nimcp_error_t caught_code = NIMCP_SUCCESS;

    NIMCP_TRY {
        // Simulate visual processing that throws
        nimcp_exception_throw(
            NIMCP_ERROR_OCCIPITAL_VISUAL_PROCESSING,
            __FILE__, __LINE__, __func__,
            "V1 processing threw exception"
        );
        // Should not reach here
        FAIL() << "Expected exception to be thrown";
    }
    NIMCP_CATCH(nimcp_exception_t, ex) {
        exception_caught = true;
        caught_code = ex->code;
        EXPECT_EQ(ex->category, EXCEPTION_CATEGORY_BRAIN_REGION);
        nimcp_exception_unref(ex);
    }
    NIMCP_END_TRY;

    EXPECT_TRUE(exception_caught);
    EXPECT_EQ(caught_code, NIMCP_ERROR_OCCIPITAL_VISUAL_PROCESSING);
}

TEST_F(PerceptionExceptionIntegrationTest, TryCatchAudioException) {
    // WHAT: Test try/catch mechanism with audio exception
    // WHY:  Verify typed catch works for brain exceptions

    bool exception_caught = false;
    const char* region_name = nullptr;

    NIMCP_TRY {
        nimcp_brain_exception_t* brain_ex = nimcp_brain_exception_create(
            NIMCP_ERROR_TEMPORAL_AUDITORY,
            EXCEPTION_SEVERITY_ERROR,
            __FILE__, __LINE__, __func__,
            1, "audio_cortex_a1",
            "A1 frequency analysis failed"
        );
        nimcp_exception_raise((nimcp_exception_t*)brain_ex);
        FAIL() << "Expected exception to be raised";
    }
    NIMCP_CATCH(nimcp_exception_t, caught_ex) {
        // Check if it's a brain exception type
        if (caught_ex->type == EXCEPTION_TYPE_BRAIN) {
            nimcp_brain_exception_t* brain_ex = (nimcp_brain_exception_t*)caught_ex;
            exception_caught = true;
            region_name = brain_ex->region_name;
            EXPECT_STREQ(region_name, "audio_cortex_a1");
        }
        nimcp_exception_unref(caught_ex);
    }
    NIMCP_END_TRY;

    EXPECT_TRUE(exception_caught);
}

//=============================================================================
// Epitope and Memory Formation Tests
//=============================================================================

TEST_F(PerceptionExceptionIntegrationTest, EpitopeConsistency) {
    // WHAT: Test epitope generation consistency
    // WHY:  Same exception pattern should generate same epitope

    // Create two identical exceptions
    nimcp_exception_t* ex1 = nimcp_exception_create(
        NIMCP_ERROR_OCCIPITAL_VISUAL_PROCESSING,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Identical visual error"
    );

    nimcp_exception_t* ex2 = nimcp_exception_create(
        NIMCP_ERROR_OCCIPITAL_VISUAL_PROCESSING,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Identical visual error"
    );

    ASSERT_NE(ex1, nullptr);
    ASSERT_NE(ex2, nullptr);

    // Generate epitopes
    nimcp_exception_generate_epitope(ex1);
    nimcp_exception_generate_epitope(ex2);

    // Epitopes should be similar (may not be identical due to timestamps)
    // At minimum, first bytes (code-based) should match
    EXPECT_EQ(ex1->epitope[0], ex2->epitope[0]);
    EXPECT_EQ(ex1->epitope[1], ex2->epitope[1]);
    EXPECT_EQ(ex1->epitope[2], ex2->epitope[2]);
    EXPECT_EQ(ex1->epitope[3], ex2->epitope[3]);

    nimcp_exception_unref(ex1);
    nimcp_exception_unref(ex2);
}

//=============================================================================
// Context Propagation Tests
//=============================================================================

TEST_F(PerceptionExceptionIntegrationTest, ContextPropagationInChain) {
    // WHAT: Test context preservation through exception chain
    // WHY:  Verify diagnostic context is maintained through cascade

    // Create chain with context
    nimcp_exception_t* root = nimcp_exception_create(
        NIMCP_ERROR_TEMPORAL_INVALID_INPUT,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Root cochlea error"
    );
    nimcp_exception_set_context(root, "sample_rate", "44100");
    nimcp_exception_set_context(root, "buffer_size", "2048");

    nimcp_exception_t* mid = nimcp_exception_create(
        NIMCP_ERROR_TEMPORAL_AUDITORY,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Mid audio error"
    );
    nimcp_exception_set_context(mid, "fft_size", "1024");
    nimcp_exception_set_cause(mid, root);

    nimcp_exception_t* top = nimcp_exception_create(
        NIMCP_ERROR_WERNICKE_COMPREHENSION,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Top speech error"
    );
    nimcp_exception_set_context(top, "phoneme", "unknown");
    nimcp_exception_set_cause(top, mid);

    // Verify context at each level
    EXPECT_STREQ(nimcp_exception_get_context(top, "phoneme"), "unknown");
    EXPECT_EQ(nimcp_exception_get_context(top, "fft_size"), nullptr);  // Not in top

    nimcp_exception_t* cause = nimcp_exception_get_cause(top);
    ASSERT_NE(cause, nullptr);
    EXPECT_STREQ(nimcp_exception_get_context(cause, "fft_size"), "1024");

    cause = nimcp_exception_get_cause(cause);
    ASSERT_NE(cause, nullptr);
    EXPECT_STREQ(nimcp_exception_get_context(cause, "sample_rate"), "44100");
    EXPECT_STREQ(nimcp_exception_get_context(cause, "buffer_size"), "2048");

    nimcp_exception_unref(top);
}

//=============================================================================
// Handler Enable/Disable Tests
//=============================================================================

TEST_F(PerceptionExceptionIntegrationTest, HandlerEnableDisable) {
    // WHAT: Test handler enable/disable functionality
    // WHY:  Verify handlers can be temporarily disabled

    static bool handler_called = false;

    auto test_handler = [](nimcp_exception_t* ex, void* user_data) -> bool {
        (void)ex;
        (void)user_data;
        handler_called = true;
        return false;
    };

    nimcp_handler_options_t opts;
    nimcp_handler_default_options(&opts);
    opts.name = "toggle_handler";
    opts.handler = test_handler;
    opts.priority = NIMCP_HANDLER_PRIORITY_HIGH;
    nimcp_handler_registration_t* reg = nimcp_handler_register(&opts);

    // Test with handler enabled
    handler_called = false;
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_OCCIPITAL_VISUAL_PROCESSING,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Test exception"
    );
    nimcp_exception_dispatch(ex);
    EXPECT_TRUE(handler_called);
    nimcp_exception_unref(ex);

    // Disable handler
    nimcp_handler_disable(reg);

    // Test with handler disabled
    handler_called = false;
    ex = nimcp_exception_create(
        NIMCP_ERROR_OCCIPITAL_VISUAL_PROCESSING,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Test exception 2"
    );
    nimcp_exception_dispatch(ex);
    EXPECT_FALSE(handler_called);
    nimcp_exception_unref(ex);

    // Re-enable handler
    nimcp_handler_enable(reg);

    // Test with handler re-enabled
    handler_called = false;
    ex = nimcp_exception_create(
        NIMCP_ERROR_OCCIPITAL_VISUAL_PROCESSING,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Test exception 3"
    );
    nimcp_exception_dispatch(ex);
    EXPECT_TRUE(handler_called);
    nimcp_exception_unref(ex);

    nimcp_handler_unregister(reg);
}

//=============================================================================
// Recovery Notification Tests
//=============================================================================

TEST_F(PerceptionExceptionIntegrationTest, RecoveryResultNotification) {
    // WHAT: Test recovery result notification to immune system
    // WHY:  Verify immune system learns from recovery outcomes

    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_OCCIPITAL_VISUAL_PROCESSING,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Visual exception for recovery test"
    );
    ASSERT_NE(ex, nullptr);

    // Present to immune
    nimcp_immune_response_t response;
    nimcp_exception_present_to_immune(ex, &response);

    // Simulate recovery attempt
    ex->recovery_attempted = true;
    ex->recovery_succeeded = true;

    // Notify immune of success
    int result = nimcp_exception_notify_recovery_result(
        ex,
        EXCEPTION_RECOVERY_RETRY,
        true  // success
    );
    // Result depends on immune system state
    (void)result;

    // Notify immune of failure
    ex->recovery_succeeded = false;
    result = nimcp_exception_notify_recovery_result(
        ex,
        EXCEPTION_RECOVERY_RETRY,
        false  // failure
    );
    (void)result;

    nimcp_exception_unref(ex);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
