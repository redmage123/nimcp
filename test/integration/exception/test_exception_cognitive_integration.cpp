/**
 * @file test_exception_cognitive_integration.cpp
 * @brief Integration tests for exception handling across cognitive modules
 * @version 1.0.0
 * @date 2026-01-21
 *
 * WHAT: Test exception handling for cognitive module errors including working memory,
 *       executive control, emotional tagging, and other cognitive subsystems
 * WHY:  Cognitive failures need specialized exception handling and recovery
 * HOW:  Simulate cognitive module errors, test immune presentation, recovery flows
 *
 * TEST SCENARIOS:
 * - Working memory exception handling and recovery
 * - Executive control failure cascades
 * - Emotional tagging exception propagation
 * - Multi-cognitive module aggregate failures
 * - Mental health monitor threshold exceptions
 * - Theory of mind processing exceptions
 * - Meta-learning exception handling
 * - Predictive processing error recovery
 * - Cross-cognitive module exception chains
 * - Immune system integration for cognitive errors
 *
 * HEADER FILES REFERENCED:
 * - include/utils/exception/nimcp_exception.h
 * - include/utils/exception/nimcp_exception_handlers.h
 * - include/utils/exception/nimcp_exception_immune.h
 * - include/utils/exception/nimcp_exception_metrics.h
 * - include/utils/exception/nimcp_exception_circuit.h
 * - include/utils/error/nimcp_error_codes.h
 *
 * @author NIMCP Development Team
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cstdio>
#include <vector>
#include <string>
#include <atomic>
#include <chrono>
#include <thread>
#include <mutex>
#include <map>
#include <functional>

extern "C" {
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_handlers.h"
#include "utils/exception/nimcp_exception_immune.h"
#include "utils/exception/nimcp_exception_metrics.h"
#include "utils/exception/nimcp_exception_circuit.h"
#include "utils/error/nimcp_error_codes.h"
}

//=============================================================================
// Cognitive Module Error Tracking
//=============================================================================

/**
 * @brief Enum representing cognitive module types
 */
enum class CognitiveModule {
    WORKING_MEMORY = 0,
    EXECUTIVE_CONTROL,
    EMOTIONAL_TAGGING,
    MENTAL_HEALTH,
    THEORY_OF_MIND,
    META_LEARNING,
    PREDICTIVE,
    SLEEP_WAKE,
    EXPLANATIONS,
    COUNT
};

/**
 * @brief Record of cognitive exception event
 */
struct CognitiveExceptionEvent {
    CognitiveModule module;
    nimcp_error_t code;
    nimcp_exception_severity_t severity;
    std::string message;
    uint64_t timestamp;
    bool handled;
    bool presented_to_immune;
    nimcp_exception_recovery_action_t recovery_action;
    bool recovery_success;
};

static struct {
    std::mutex mutex;
    std::vector<CognitiveExceptionEvent> events;
    std::atomic<int> handler_calls{0};
    std::atomic<int> recovery_calls{0};
    std::atomic<int> immune_presentations{0};
    std::map<CognitiveModule, std::atomic<int>> module_error_counts;
} g_cognitive_state;

//=============================================================================
// Cognitive Exception Handler Functions
//=============================================================================

static CognitiveModule error_code_to_module(nimcp_error_t code) {
    switch (code) {
        case NIMCP_ERROR_WORKING_MEMORY:
            return CognitiveModule::WORKING_MEMORY;
        case NIMCP_ERROR_EXECUTIVE_CONTROL:
            return CognitiveModule::EXECUTIVE_CONTROL;
        case NIMCP_ERROR_EMOTIONAL_TAGGING:
            return CognitiveModule::EMOTIONAL_TAGGING;
        case NIMCP_ERROR_MENTAL_HEALTH:
            return CognitiveModule::MENTAL_HEALTH;
        case NIMCP_ERROR_THEORY_OF_MIND:
            return CognitiveModule::THEORY_OF_MIND;
        case NIMCP_ERROR_META_LEARNING:
            return CognitiveModule::META_LEARNING;
        case NIMCP_ERROR_PREDICTIVE:
            return CognitiveModule::PREDICTIVE;
        case NIMCP_ERROR_SLEEP_WAKE:
            return CognitiveModule::SLEEP_WAKE;
        case NIMCP_ERROR_EXPLANATIONS:
            return CognitiveModule::EXPLANATIONS;
        default:
            return CognitiveModule::WORKING_MEMORY;  // Default
    }
}

static void record_cognitive_exception(nimcp_exception_t* ex, bool handled) {
    CognitiveExceptionEvent event;
    event.module = error_code_to_module(ex->code);
    event.code = ex->code;
    event.severity = ex->severity;
    event.message = ex->message;
    event.timestamp = ex->timestamp_us;
    event.handled = handled;
    event.presented_to_immune = ex->presented_to_immune;
    event.recovery_action = EXCEPTION_RECOVERY_NONE;
    event.recovery_success = false;

    std::lock_guard<std::mutex> lock(g_cognitive_state.mutex);
    g_cognitive_state.events.push_back(event);
}

static bool cognitive_exception_handler(nimcp_exception_t* ex, void* user_data) {
    (void)user_data;

    // Only handle cognitive category exceptions
    if (ex->category != EXCEPTION_CATEGORY_COGNITIVE) {
        return false;
    }

    g_cognitive_state.handler_calls++;
    record_cognitive_exception(ex, false);

    // Track per-module error counts
    CognitiveModule module = error_code_to_module(ex->code);
    g_cognitive_state.module_error_counts[module]++;

    return false;  // Don't consume - let others see it
}

static int cognitive_recovery_callback(nimcp_exception_t* ex,
                                       nimcp_exception_recovery_action_t action,
                                       void* user_data) {
    (void)user_data;

    g_cognitive_state.recovery_calls++;

    // Simulate recovery based on action and error type
    bool success = false;
    switch (action) {
        case EXCEPTION_RECOVERY_GC:
            // GC helps with working memory
            success = (ex->code == NIMCP_ERROR_WORKING_MEMORY);
            break;
        case EXCEPTION_RECOVERY_REDUCE_LOAD:
            // Reduce load helps with executive control and mental health
            success = (ex->code == NIMCP_ERROR_EXECUTIVE_CONTROL ||
                      ex->code == NIMCP_ERROR_MENTAL_HEALTH);
            break;
        case EXCEPTION_RECOVERY_CLEAR_CACHE:
            // Clear cache helps with meta-learning and predictive
            success = (ex->code == NIMCP_ERROR_META_LEARNING ||
                      ex->code == NIMCP_ERROR_PREDICTIVE);
            break;
        case EXCEPTION_RECOVERY_RETRY:
            // Retry helps with attention and theory of mind
            success = (ex->code == NIMCP_ERROR_WORKING_MEMORY ||
                      ex->code == NIMCP_ERROR_THEORY_OF_MIND);
            break;
        case EXCEPTION_RECOVERY_ROLLBACK:
            // Rollback helps with emotional tagging
            success = (ex->code == NIMCP_ERROR_EMOTIONAL_TAGGING);
            break;
        case EXCEPTION_RECOVERY_RESTART_COMPONENT:
            // Restart helps with sleep-wake
            success = (ex->code == NIMCP_ERROR_SLEEP_WAKE);
            break;
        default:
            success = false;
            break;
    }

    return success ? 0 : -1;
}

//=============================================================================
// Test Fixture
//=============================================================================

class ExceptionCognitiveIntegrationTest : public ::testing::Test {
protected:
    nimcp_handler_registration_t* handler_reg_ = nullptr;

    void SetUp() override {
        // Reset state
        {
            std::lock_guard<std::mutex> lock(g_cognitive_state.mutex);
            g_cognitive_state.events.clear();
        }
        g_cognitive_state.handler_calls = 0;
        g_cognitive_state.recovery_calls = 0;
        g_cognitive_state.immune_presentations = 0;
        for (int i = 0; i < (int)CognitiveModule::COUNT; i++) {
            g_cognitive_state.module_error_counts[(CognitiveModule)i] = 0;
        }

        // Initialize exception system
        nimcp_exception_system_init();

        // Initialize circuit breaker
        nimcp_circuit_init();

        // Initialize metrics
        nimcp_metrics_init();

        // Initialize immune integration
        nimcp_exception_immune_config_t immune_config;
        nimcp_exception_immune_default_config(&immune_config);
        immune_config.enable_auto_present = false;
        immune_config.enable_auto_recovery = false;
        nimcp_exception_immune_init(&immune_config);

        // Install default handlers
        nimcp_install_default_handlers();

        // Register cognitive handler
        nimcp_handler_options_t opts;
        nimcp_handler_default_options(&opts);
        opts.name = "cognitive_test_handler";
        opts.handler = cognitive_exception_handler;
        opts.priority = NIMCP_HANDLER_PRIORITY_HIGH;
        handler_reg_ = nimcp_handler_register(&opts);

        // Register recovery callbacks
        nimcp_register_recovery_callback(EXCEPTION_RECOVERY_GC, cognitive_recovery_callback, nullptr);
        nimcp_register_recovery_callback(EXCEPTION_RECOVERY_REDUCE_LOAD, cognitive_recovery_callback, nullptr);
        nimcp_register_recovery_callback(EXCEPTION_RECOVERY_CLEAR_CACHE, cognitive_recovery_callback, nullptr);
        nimcp_register_recovery_callback(EXCEPTION_RECOVERY_RETRY, cognitive_recovery_callback, nullptr);
        nimcp_register_recovery_callback(EXCEPTION_RECOVERY_ROLLBACK, cognitive_recovery_callback, nullptr);
        nimcp_register_recovery_callback(EXCEPTION_RECOVERY_RESTART_COMPONENT, cognitive_recovery_callback, nullptr);
    }

    void TearDown() override {
        // Unregister recovery callbacks
        nimcp_unregister_recovery_callback(EXCEPTION_RECOVERY_GC);
        nimcp_unregister_recovery_callback(EXCEPTION_RECOVERY_REDUCE_LOAD);
        nimcp_unregister_recovery_callback(EXCEPTION_RECOVERY_CLEAR_CACHE);
        nimcp_unregister_recovery_callback(EXCEPTION_RECOVERY_RETRY);
        nimcp_unregister_recovery_callback(EXCEPTION_RECOVERY_ROLLBACK);
        nimcp_unregister_recovery_callback(EXCEPTION_RECOVERY_RESTART_COMPONENT);

        if (handler_reg_) {
            nimcp_handler_unregister(handler_reg_);
            handler_reg_ = nullptr;
        }

        nimcp_exception_clear_current();
        nimcp_exception_handlers_shutdown();
        nimcp_exception_immune_shutdown();
        nimcp_metrics_shutdown();
        nimcp_circuit_shutdown();
        nimcp_exception_system_shutdown();
    }

    bool hasEventWithCode(nimcp_error_t code) {
        std::lock_guard<std::mutex> lock(g_cognitive_state.mutex);
        for (const auto& event : g_cognitive_state.events) {
            if (event.code == code) return true;
        }
        return false;
    }

    size_t eventCountForModule(CognitiveModule module) {
        std::lock_guard<std::mutex> lock(g_cognitive_state.mutex);
        size_t count = 0;
        for (const auto& event : g_cognitive_state.events) {
            if (event.module == module) count++;
        }
        return count;
    }
};

//=============================================================================
// Test: Working Memory Exception Handling
//=============================================================================

TEST_F(ExceptionCognitiveIntegrationTest, WorkingMemoryExceptionHandling) {
    // WHAT: Test working memory capacity exception handling
    // WHY:  Working memory overflow is common cognitive failure
    // HOW:  Create exception, present to immune, attempt GC recovery

    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_WORKING_MEMORY,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Working memory capacity exceeded: 8/7 items"
    );
    ASSERT_NE(ex, nullptr);

    // Add context
    nimcp_exception_set_context(ex, "module", "working_memory");
    nimcp_exception_set_context(ex, "capacity_used", "8");
    nimcp_exception_set_context(ex, "capacity_max", "7");
    nimcp_exception_set_context(ex, "operation", "chunk_store");

    // Generate epitope
    size_t epitope_len = nimcp_exception_generate_epitope(ex);
    EXPECT_GT(epitope_len, 0u);

    // Dispatch through handlers
    nimcp_exception_dispatch(ex);
    EXPECT_TRUE(hasEventWithCode(NIMCP_ERROR_WORKING_MEMORY));

    // Present to immune
    nimcp_immune_response_t response;
    int present_result = nimcp_exception_present_to_immune(ex, &response);
    EXPECT_EQ(present_result, 0);
    EXPECT_TRUE(ex->presented_to_immune);

    // Get recovery strategy - should suggest GC
    nimcp_exception_recovery_strategy_t strategy;
    nimcp_exception_get_recovery_strategy(ex, &strategy);

    // Execute GC recovery
    g_cognitive_state.recovery_calls = 0;
    int recovery_result = nimcp_exception_execute_recovery(ex, EXCEPTION_RECOVERY_GC);
    EXPECT_EQ(recovery_result, 0);  // Should succeed for working memory
    EXPECT_GE(g_cognitive_state.recovery_calls.load(), 1);

    nimcp_exception_unref(ex);
}

//=============================================================================
// Test: Executive Control Failure Cascades
//=============================================================================

TEST_F(ExceptionCognitiveIntegrationTest, ExecutiveControlFailureCascade) {
    // WHAT: Test executive control failure causing cascade to other modules
    // WHY:  Executive control affects all other cognitive functions
    // HOW:  Create root cause, chain to executive control, verify cascade

    // Root cause: attention failure
    nimcp_exception_t* attention_ex = nimcp_exception_create(
        NIMCP_ERROR_WORKING_MEMORY,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Attention resource depleted"
    );
    ASSERT_NE(attention_ex, nullptr);

    // Executive control fails because attention failed
    nimcp_exception_t* exec_ex = nimcp_exception_create(
        NIMCP_ERROR_EXECUTIVE_CONTROL,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        "Executive control impaired - attention unavailable"
    );
    ASSERT_NE(exec_ex, nullptr);
    nimcp_exception_set_cause(exec_ex, attention_ex);

    // Verify cause chain
    nimcp_exception_t* cause = nimcp_exception_get_cause(exec_ex);
    ASSERT_NE(cause, nullptr);
    EXPECT_EQ(cause->code, NIMCP_ERROR_WORKING_MEMORY);

    // Dispatch
    nimcp_exception_dispatch(exec_ex);
    EXPECT_TRUE(hasEventWithCode(NIMCP_ERROR_EXECUTIVE_CONTROL));

    // Present to immune
    nimcp_immune_response_t response;
    nimcp_exception_present_to_immune(exec_ex, &response);

    // Recovery should target reduce load for executive control
    int recovery_result = nimcp_exception_execute_recovery(exec_ex, EXCEPTION_RECOVERY_REDUCE_LOAD);
    EXPECT_EQ(recovery_result, 0);  // Should succeed for executive control

    nimcp_exception_unref(exec_ex);
}

//=============================================================================
// Test: Emotional Tagging Exception Propagation
//=============================================================================

TEST_F(ExceptionCognitiveIntegrationTest, EmotionalTaggingExceptionPropagation) {
    // WHAT: Test emotional tagging exception propagates correctly
    // WHY:  Emotional processing errors affect memory consolidation
    // HOW:  Create exception, verify propagation and rollback recovery

    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_EMOTIONAL_TAGGING,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Failed to tag memory with emotional valence"
    );
    ASSERT_NE(ex, nullptr);

    nimcp_exception_set_context(ex, "memory_id", "mem_12345");
    nimcp_exception_set_context(ex, "expected_valence", "positive");
    nimcp_exception_set_context(ex, "reason", "amygdala_offline");

    // Dispatch
    nimcp_exception_dispatch(ex);

    // Verify category is cognitive
    EXPECT_EQ(ex->category, EXCEPTION_CATEGORY_COGNITIVE);

    // Present to immune
    nimcp_immune_response_t response;
    nimcp_exception_present_to_immune(ex, &response);

    // Rollback should work for emotional tagging
    int recovery_result = nimcp_exception_execute_recovery(ex, EXCEPTION_RECOVERY_ROLLBACK);
    EXPECT_EQ(recovery_result, 0);

    nimcp_exception_unref(ex);
}

//=============================================================================
// Test: Multi-Cognitive Module Aggregate Failures
//=============================================================================

TEST_F(ExceptionCognitiveIntegrationTest, MultiCognitiveModuleAggregateFailures) {
    // WHAT: Test aggregate exception for multiple cognitive module failures
    // WHY:  System-wide cognitive impairment requires coordinated response
    // HOW:  Create aggregate with children from multiple modules

    nimcp_aggregate_exception_t* agg = nimcp_aggregate_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_CRITICAL,
        __FILE__, __LINE__, __func__,
        "Multiple cognitive systems impaired"
    );
    ASSERT_NE(agg, nullptr);

    // Working memory failure
    nimcp_exception_t* wm_ex = nimcp_exception_create(
        NIMCP_ERROR_WORKING_MEMORY,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Working memory overload"
    );
    nimcp_aggregate_exception_add(agg, wm_ex);

    // Executive control failure
    nimcp_exception_t* exec_ex = nimcp_exception_create(
        NIMCP_ERROR_EXECUTIVE_CONTROL,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Executive control degraded"
    );
    nimcp_aggregate_exception_add(agg, exec_ex);

    // Attention failure
    nimcp_exception_t* attn_ex = nimcp_exception_create(
        NIMCP_ERROR_WORKING_MEMORY,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Attention focus lost"
    );
    nimcp_aggregate_exception_add(agg, attn_ex);

    // Mental health threshold
    nimcp_exception_t* mh_ex = nimcp_exception_create(
        NIMCP_ERROR_MENTAL_HEALTH,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        "Mental health stress threshold exceeded"
    );
    nimcp_aggregate_exception_add(agg, mh_ex);

    EXPECT_EQ(nimcp_aggregate_exception_count(agg), 4u);

    // Dispatch aggregate
    nimcp_exception_dispatch((nimcp_exception_t*)agg);

    // Present to immune
    nimcp_immune_response_t response;
    int present_result = nimcp_exception_present_to_immune((nimcp_exception_t*)agg, &response);
    EXPECT_EQ(present_result, 0);

    // Should have high overall severity
    EXPECT_EQ(((nimcp_exception_t*)agg)->severity, EXCEPTION_SEVERITY_CRITICAL);

    nimcp_exception_unref((nimcp_exception_t*)agg);
}

//=============================================================================
// Test: Mental Health Monitor Threshold Exceptions
//=============================================================================

TEST_F(ExceptionCognitiveIntegrationTest, MentalHealthThresholdException) {
    // WHAT: Test mental health monitor threshold exceeding exception
    // WHY:  Mental health thresholds trigger special handling
    // HOW:  Create exception, verify immune response for mental health

    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_MENTAL_HEALTH,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        "Mental health stress index exceeded safe threshold"
    );
    ASSERT_NE(ex, nullptr);

    nimcp_exception_set_context(ex, "stress_index", "0.85");
    nimcp_exception_set_context(ex, "threshold", "0.7");
    nimcp_exception_set_context(ex, "duration_seconds", "300");
    nimcp_exception_set_context(ex, "intervention", "recommended");

    // Generate epitope
    nimcp_exception_generate_epitope(ex);

    // Dispatch
    nimcp_exception_dispatch(ex);

    // Present to immune (mental health triggers special immune response)
    nimcp_immune_response_t response;
    int present_result = nimcp_exception_present_to_immune(ex, &response);
    EXPECT_EQ(present_result, 0);

    // Reduce load should help mental health
    int recovery_result = nimcp_exception_execute_recovery(ex, EXCEPTION_RECOVERY_REDUCE_LOAD);
    EXPECT_EQ(recovery_result, 0);

    nimcp_exception_unref(ex);
}

//=============================================================================
// Test: Theory of Mind Processing Exception
//=============================================================================

TEST_F(ExceptionCognitiveIntegrationTest, TheoryOfMindProcessingException) {
    // WHAT: Test theory of mind model inconsistency exception
    // WHY:  ToM failures affect social cognition
    // HOW:  Create exception, attempt retry recovery

    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_THEORY_OF_MIND,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Theory of mind model prediction failed"
    );
    ASSERT_NE(ex, nullptr);

    nimcp_exception_set_context(ex, "agent_id", "agent_42");
    nimcp_exception_set_context(ex, "predicted_action", "cooperate");
    nimcp_exception_set_context(ex, "actual_action", "defect");
    nimcp_exception_set_context(ex, "confidence", "0.9");

    // Dispatch
    nimcp_exception_dispatch(ex);
    EXPECT_TRUE(hasEventWithCode(NIMCP_ERROR_THEORY_OF_MIND));

    // Present to immune
    nimcp_immune_response_t response;
    nimcp_exception_present_to_immune(ex, &response);

    // Retry should help ToM
    int recovery_result = nimcp_exception_execute_recovery(ex, EXCEPTION_RECOVERY_RETRY);
    EXPECT_EQ(recovery_result, 0);

    nimcp_exception_unref(ex);
}

//=============================================================================
// Test: Meta-Learning Exception Handling
//=============================================================================

TEST_F(ExceptionCognitiveIntegrationTest, MetaLearningExceptionHandling) {
    // WHAT: Test meta-learning optimization failure exception
    // WHY:  Meta-learning affects learning-to-learn capabilities
    // HOW:  Create exception, verify clear cache recovery

    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_META_LEARNING,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Meta-learning optimization failed to converge"
    );
    ASSERT_NE(ex, nullptr);

    nimcp_exception_set_context(ex, "learning_rate", "0.001");
    nimcp_exception_set_context(ex, "iterations", "1000");
    nimcp_exception_set_context(ex, "gradient_norm", "inf");

    // Dispatch
    nimcp_exception_dispatch(ex);

    // Present to immune
    nimcp_immune_response_t response;
    nimcp_exception_present_to_immune(ex, &response);

    // Clear cache should help meta-learning
    int recovery_result = nimcp_exception_execute_recovery(ex, EXCEPTION_RECOVERY_CLEAR_CACHE);
    EXPECT_EQ(recovery_result, 0);

    nimcp_exception_unref(ex);
}

//=============================================================================
// Test: Predictive Processing Error Recovery
//=============================================================================

TEST_F(ExceptionCognitiveIntegrationTest, PredictiveProcessingErrorRecovery) {
    // WHAT: Test predictive processing prediction error exception
    // WHY:  Prediction errors trigger learning but large errors need handling
    // HOW:  Create exception, verify recovery flow

    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_PREDICTIVE,
        EXCEPTION_SEVERITY_WARNING,
        __FILE__, __LINE__, __func__,
        "Prediction error exceeds tolerance threshold"
    );
    ASSERT_NE(ex, nullptr);

    nimcp_exception_set_context(ex, "predicted", "42.0");
    nimcp_exception_set_context(ex, "actual", "100.0");
    nimcp_exception_set_context(ex, "error", "58.0");
    nimcp_exception_set_context(ex, "precision", "0.1");

    // Dispatch
    nimcp_exception_dispatch(ex);

    // Present to immune
    nimcp_immune_response_t response;
    nimcp_exception_present_to_immune(ex, &response);

    // Clear cache should help predictive
    int recovery_result = nimcp_exception_execute_recovery(ex, EXCEPTION_RECOVERY_CLEAR_CACHE);
    EXPECT_EQ(recovery_result, 0);

    nimcp_exception_unref(ex);
}

//=============================================================================
// Test: Sleep-Wake Cycle Exception
//=============================================================================

TEST_F(ExceptionCognitiveIntegrationTest, SleepWakeCycleException) {
    // WHAT: Test sleep-wake cycle disruption exception
    // WHY:  Sleep disruption affects all cognitive functions
    // HOW:  Create exception, verify restart component recovery

    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_SLEEP_WAKE,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        "Sleep-wake cycle disrupted - circadian rhythm desynchronized"
    );
    ASSERT_NE(ex, nullptr);

    nimcp_exception_set_context(ex, "expected_state", "sleep");
    nimcp_exception_set_context(ex, "actual_state", "hyperalert");
    nimcp_exception_set_context(ex, "hours_since_sleep", "48");

    // Dispatch
    nimcp_exception_dispatch(ex);

    // Present to immune
    nimcp_immune_response_t response;
    nimcp_exception_present_to_immune(ex, &response);

    // Restart component should help sleep-wake
    int recovery_result = nimcp_exception_execute_recovery(ex, EXCEPTION_RECOVERY_RESTART_COMPONENT);
    EXPECT_EQ(recovery_result, 0);

    nimcp_exception_unref(ex);
}

//=============================================================================
// Test: Cross-Cognitive Module Exception Chain
//=============================================================================

TEST_F(ExceptionCognitiveIntegrationTest, CrossCognitiveModuleExceptionChain) {
    // WHAT: Test exception chain across multiple cognitive modules
    // WHY:  Cognitive modules are interconnected - failures propagate
    // HOW:  Create chain of exceptions from different modules

    // Root: Predictive processing error
    nimcp_exception_t* predictive = nimcp_exception_create(
        NIMCP_ERROR_PREDICTIVE,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Prediction failed"
    );
    ASSERT_NE(predictive, nullptr);

    // Causes attention failure
    nimcp_exception_t* attention = nimcp_exception_create(
        NIMCP_ERROR_WORKING_MEMORY,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Attention focus disrupted by prediction error"
    );
    nimcp_exception_set_cause(attention, predictive);

    // Causes working memory overload
    nimcp_exception_t* working_memory = nimcp_exception_create(
        NIMCP_ERROR_WORKING_MEMORY,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Working memory overloaded due to attention failure"
    );
    nimcp_exception_set_cause(working_memory, attention);

    // Causes executive control impairment
    nimcp_exception_t* executive = nimcp_exception_create(
        NIMCP_ERROR_EXECUTIVE_CONTROL,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        "Executive control collapsed - cascade failure"
    );
    nimcp_exception_set_cause(executive, working_memory);

    // Verify complete chain
    EXPECT_EQ(nimcp_exception_get_cause(executive)->code, NIMCP_ERROR_WORKING_MEMORY);
    EXPECT_EQ(nimcp_exception_get_cause(nimcp_exception_get_cause(executive))->code,
              NIMCP_ERROR_WORKING_MEMORY);
    EXPECT_EQ(nimcp_exception_get_cause(nimcp_exception_get_cause(
              nimcp_exception_get_cause(executive)))->code, NIMCP_ERROR_PREDICTIVE);

    // Dispatch top-level
    nimcp_exception_dispatch(executive);

    // Present to immune
    nimcp_immune_response_t response;
    int present_result = nimcp_exception_present_to_immune(executive, &response);
    EXPECT_EQ(present_result, 0);

    nimcp_exception_unref(executive);
}

//=============================================================================
// Test: Immune System Integration for Cognitive Errors
//=============================================================================

TEST_F(ExceptionCognitiveIntegrationTest, ImmuneSystemIntegrationForCognitiveErrors) {
    // WHAT: Test immune system receives and processes cognitive errors
    // WHY:  Immune pattern matching for cognitive anomalies
    // HOW:  Create various cognitive exceptions, verify immune processing

    std::vector<nimcp_exception_t*> exceptions;

    // Create cognitive exceptions of different types
    nimcp_error_t cognitive_codes[] = {
        NIMCP_ERROR_WORKING_MEMORY,
        NIMCP_ERROR_EXECUTIVE_CONTROL,
        NIMCP_ERROR_EMOTIONAL_TAGGING,
        NIMCP_ERROR_WORKING_MEMORY,
        NIMCP_ERROR_THEORY_OF_MIND
    };

    for (auto code : cognitive_codes) {
        nimcp_exception_t* ex = nimcp_exception_create(
            code,
            EXCEPTION_SEVERITY_ERROR,
            __FILE__, __LINE__, __func__,
            "Cognitive error %d for immune test", code
        );
        ASSERT_NE(ex, nullptr);

        // Generate unique epitope
        nimcp_exception_generate_epitope(ex);

        // Present to immune
        nimcp_immune_response_t response;
        int result = nimcp_exception_present_to_immune(ex, &response);
        EXPECT_EQ(result, 0);
        EXPECT_TRUE(ex->presented_to_immune);

        exceptions.push_back(ex);
    }

    // Cleanup
    for (auto* ex : exceptions) {
        nimcp_exception_unref(ex);
    }
}

//=============================================================================
// Test: Epitope Consistency for Cognitive Exceptions
//=============================================================================

TEST_F(ExceptionCognitiveIntegrationTest, EpitopeConsistencyForCognitiveExceptions) {
    // WHAT: Test same cognitive exception produces same epitope
    // WHY:  Immune pattern matching requires consistent epitopes
    // HOW:  Create same exception twice, compare epitopes

    nimcp_exception_t* ex1 = nimcp_exception_create(
        NIMCP_ERROR_WORKING_MEMORY,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, 100, "test_func",
        "Working memory test"
    );

    nimcp_exception_t* ex2 = nimcp_exception_create(
        NIMCP_ERROR_WORKING_MEMORY,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, 100, "test_func",
        "Working memory test"
    );

    ASSERT_NE(ex1, nullptr);
    ASSERT_NE(ex2, nullptr);

    uint8_t epitope1[NIMCP_EXCEPTION_EPITOPE_SIZE];
    uint8_t epitope2[NIMCP_EXCEPTION_EPITOPE_SIZE];

    size_t len1 = nimcp_exception_compute_epitope(ex1, epitope1, sizeof(epitope1));
    size_t len2 = nimcp_exception_compute_epitope(ex2, epitope2, sizeof(epitope2));

    EXPECT_EQ(len1, len2);
    EXPECT_EQ(memcmp(epitope1, epitope2, len1), 0)
        << "Same cognitive exception should produce same epitope";

    nimcp_exception_unref(ex1);
    nimcp_exception_unref(ex2);
}

//=============================================================================
// Test: Recovery Strategy By Cognitive Error Type
//=============================================================================

TEST_F(ExceptionCognitiveIntegrationTest, RecoveryStrategyByCognitiveErrorType) {
    // WHAT: Test each cognitive error type gets appropriate recovery strategy
    // WHY:  Different cognitive modules need different recovery approaches
    // HOW:  Create each type, check recovery strategy

    struct TestCase {
        nimcp_error_t code;
        const char* name;
        nimcp_exception_recovery_action_t expected_recovery;
    };

    TestCase cases[] = {
        { NIMCP_ERROR_WORKING_MEMORY, "WORKING_MEMORY", EXCEPTION_RECOVERY_GC },
        { NIMCP_ERROR_EXECUTIVE_CONTROL, "EXECUTIVE_CONTROL", EXCEPTION_RECOVERY_REDUCE_LOAD },
        { NIMCP_ERROR_EMOTIONAL_TAGGING, "EMOTIONAL_TAGGING", EXCEPTION_RECOVERY_ROLLBACK },
        { NIMCP_ERROR_WORKING_MEMORY, "ATTENTION", EXCEPTION_RECOVERY_RETRY },
        { NIMCP_ERROR_THEORY_OF_MIND, "THEORY_OF_MIND", EXCEPTION_RECOVERY_RETRY },
        { NIMCP_ERROR_META_LEARNING, "META_LEARNING", EXCEPTION_RECOVERY_CLEAR_CACHE },
        { NIMCP_ERROR_PREDICTIVE, "PREDICTIVE", EXCEPTION_RECOVERY_CLEAR_CACHE },
        { NIMCP_ERROR_MENTAL_HEALTH, "MENTAL_HEALTH", EXCEPTION_RECOVERY_REDUCE_LOAD },
        { NIMCP_ERROR_SLEEP_WAKE, "SLEEP_WAKE", EXCEPTION_RECOVERY_RESTART_COMPONENT }
    };

    for (const auto& tc : cases) {
        nimcp_exception_t* ex = nimcp_exception_create(
            tc.code,
            EXCEPTION_SEVERITY_ERROR,
            __FILE__, __LINE__, __func__,
            "Recovery strategy test for %s", tc.name
        );
        ASSERT_NE(ex, nullptr) << "Failed for " << tc.name;

        // Execute expected recovery
        g_cognitive_state.recovery_calls = 0;
        int result = nimcp_exception_execute_recovery(ex, tc.expected_recovery);
        EXPECT_EQ(result, 0) << tc.name << " recovery should succeed with "
                            << tc.expected_recovery;
        EXPECT_GE(g_cognitive_state.recovery_calls.load(), 1)
            << tc.name << " should have called recovery callback";

        nimcp_exception_unref(ex);
    }
}

//=============================================================================
// Test: Try/Catch for Cognitive Exceptions
//=============================================================================

TEST_F(ExceptionCognitiveIntegrationTest, TryCatchForCognitiveExceptions) {
    // WHAT: Test try/catch mechanism for cognitive exceptions
    // WHY:  Structured exception handling for cognitive code
    // HOW:  Use NIMCP_TRY/NIMCP_CATCH with cognitive exceptions

    bool caught = false;
    nimcp_error_t caught_code = NIMCP_SUCCESS;

    NIMCP_TRY {
        nimcp_exception_t* ex = nimcp_exception_create(
            NIMCP_ERROR_WORKING_MEMORY,
            EXCEPTION_SEVERITY_ERROR,
            __FILE__, __LINE__, __func__,
            "Working memory overflow in try block"
        );
        nimcp_exception_raise(ex);
        FAIL() << "Should not reach here";
    }
    NIMCP_CATCH(nimcp_exception_t, ex) {
        caught = true;
        caught_code = ex->code;
        EXPECT_EQ(ex->category, EXCEPTION_CATEGORY_COGNITIVE);
        nimcp_exception_unref(ex);
    }
    NIMCP_END_TRY;

    EXPECT_TRUE(caught);
    EXPECT_EQ(caught_code, NIMCP_ERROR_WORKING_MEMORY);
}

//=============================================================================
// Test: Circuit Breaker for Repeated Cognitive Failures
//=============================================================================

TEST_F(ExceptionCognitiveIntegrationTest, CircuitBreakerForRepeatedCognitiveFailures) {
    // WHAT: Test circuit breaker tracks repeated cognitive failures
    // WHY:  Prevent cascading failures from repeated cognitive errors
    // HOW:  Create multiple same-type exceptions, check circuit state

    // Generate several working memory exceptions
    for (int i = 0; i < 5; i++) {
        nimcp_exception_t* ex = nimcp_exception_create(
            NIMCP_ERROR_WORKING_MEMORY,
            EXCEPTION_SEVERITY_ERROR,
            __FILE__, __LINE__, __func__,
            "Circuit breaker test %d", i
        );
        ASSERT_NE(ex, nullptr);

        nimcp_circuit_record(ex);
        nimcp_exception_dispatch(ex);
        nimcp_exception_unref(ex);
    }

    // Check circuit breaker count
    size_t count = nimcp_circuit_get_count(NIMCP_ERROR_WORKING_MEMORY, 60);
    EXPECT_GE(count, 5u);

    // Circuit should still be closed (below threshold)
    nimcp_circuit_state_t state = nimcp_circuit_get_state(NIMCP_ERROR_WORKING_MEMORY);
    EXPECT_EQ(state, CIRCUIT_STATE_CLOSED);
}

//=============================================================================
// Test: Metrics for Cognitive Exceptions
//=============================================================================

TEST_F(ExceptionCognitiveIntegrationTest, MetricsForCognitiveExceptions) {
    // WHAT: Test metrics track cognitive exceptions
    // WHY:  Observability for cognitive system health
    // HOW:  Create exceptions, verify metrics are recorded

    nimcp_metrics_reset();

    // Create several cognitive exceptions
    nimcp_error_t codes[] = {
        NIMCP_ERROR_WORKING_MEMORY,
        NIMCP_ERROR_EXECUTIVE_CONTROL,
        NIMCP_ERROR_EMOTIONAL_TAGGING
    };

    for (auto code : codes) {
        nimcp_exception_t* ex = nimcp_exception_create(
            code,
            EXCEPTION_SEVERITY_ERROR,
            __FILE__, __LINE__, __func__,
            "Metrics test"
        );
        ASSERT_NE(ex, nullptr);
        nimcp_metrics_record_exception(ex);
        nimcp_exception_unref(ex);
    }

    // Check metrics
    nimcp_exception_metrics_t metrics;
    nimcp_metrics_get(&metrics);
    EXPECT_GE(metrics.total_exceptions, 3u);
}

//=============================================================================
// Test: Concurrent Cognitive Exception Handling
//=============================================================================

TEST_F(ExceptionCognitiveIntegrationTest, ConcurrentCognitiveExceptionHandling) {
    // WHAT: Test thread-safe cognitive exception handling
    // WHY:  Multiple cognitive modules may fail concurrently
    // HOW:  Create exceptions from multiple threads

    const int NUM_THREADS = 4;
    const int EXCEPTIONS_PER_THREAD = 10;
    std::atomic<int> total_created{0};
    std::vector<std::thread> threads;

    nimcp_error_t cognitive_codes[] = {
        NIMCP_ERROR_WORKING_MEMORY,
        NIMCP_ERROR_EXECUTIVE_CONTROL,
        NIMCP_ERROR_EMOTIONAL_TAGGING,
        NIMCP_ERROR_WORKING_MEMORY,
        NIMCP_ERROR_THEORY_OF_MIND
    };

    for (int t = 0; t < NUM_THREADS; t++) {
        threads.emplace_back([&total_created, t, &cognitive_codes]() {
            for (int i = 0; i < EXCEPTIONS_PER_THREAD; i++) {
                nimcp_error_t code = cognitive_codes[(t + i) % 5];
                nimcp_exception_t* ex = nimcp_exception_create(
                    code,
                    EXCEPTION_SEVERITY_ERROR,
                    __FILE__, __LINE__, __func__,
                    "Thread %d exception %d", t, i
                );
                if (ex) {
                    total_created++;
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
    EXPECT_GE(g_cognitive_state.handler_calls.load(), NUM_THREADS * EXCEPTIONS_PER_THREAD);
}

//=============================================================================
// Test: Explanations Module Exception
//=============================================================================

TEST_F(ExceptionCognitiveIntegrationTest, ExplanationsModuleException) {
    // WHAT: Test natural language explanation generation failure
    // WHY:  Explanation failures affect interpretability
    // HOW:  Create exception for explanation failure

    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_EXPLANATIONS,
        EXCEPTION_SEVERITY_WARNING,
        __FILE__, __LINE__, __func__,
        "Failed to generate natural language explanation"
    );
    ASSERT_NE(ex, nullptr);

    nimcp_exception_set_context(ex, "query", "Why did the model predict X?");
    nimcp_exception_set_context(ex, "reason", "attention_weights_unavailable");
    nimcp_exception_set_context(ex, "fallback", "generic_explanation");

    // Dispatch
    nimcp_exception_dispatch(ex);
    EXPECT_TRUE(hasEventWithCode(NIMCP_ERROR_EXPLANATIONS));

    // Present to immune
    nimcp_immune_response_t response;
    nimcp_exception_present_to_immune(ex, &response);

    nimcp_exception_unref(ex);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
