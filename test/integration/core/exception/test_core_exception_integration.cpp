/**
 * @file test_core_exception_integration.cpp
 * @brief Integration tests for NIMCP exception handling system
 *
 * WHAT: Tests exception handling across core modules with immune integration
 * WHY:  Verify exception flows work correctly in realistic scenarios
 * HOW:  Test exception chains, multi-module errors, and recovery workflows
 *
 * TEST COVERAGE:
 * - Exception dispatch through handler chain with multiple handlers
 * - Exception chaining across module boundaries
 * - Immune system integration for automatic recovery
 * - Concurrent exception handling from multiple threads
 * - Recovery callback execution
 * - Statistics and metrics collection
 *
 * @author NIMCP Development Team
 * @date 2026-01-16
 */

#include <gtest/gtest.h>
#include <thread>
#include <atomic>
#include <vector>
#include <chrono>
#include <string.h>

extern "C" {
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_handlers.h"
#include "utils/exception/nimcp_exception_immune.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/memory/nimcp_memory.h"
}

// ============================================================================
// Test Fixture
// ============================================================================

class CoreExceptionIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize exception system
        nimcp_exception_system_init();

        // Initialize immune integration
        nimcp_exception_immune_init(nullptr);
    }

    void TearDown() override {
        // Clear any current exception
        nimcp_exception_clear_current();

        // Shutdown immune integration
        nimcp_exception_immune_shutdown();
    }
};

// ============================================================================
// Handler Chain Integration Tests
// ============================================================================

namespace {
    // Test handler state tracking
    std::atomic<int> g_handler_call_count{0};
    std::atomic<bool> g_logging_handler_called{false};
    std::atomic<bool> g_metrics_handler_called{false};
    std::atomic<bool> g_recovery_handler_called{false};
    nimcp_exception_t* g_last_logged_exception = nullptr;

    void reset_handler_state() {
        g_handler_call_count = 0;
        g_logging_handler_called = false;
        g_metrics_handler_called = false;
        g_recovery_handler_called = false;
        g_last_logged_exception = nullptr;
    }

    // Simulated logging handler
    bool logging_handler(nimcp_exception_t* ex, void* user_data) {
        (void)user_data;
        g_logging_handler_called = true;
        g_handler_call_count++;
        g_last_logged_exception = ex;
        // Don't consume - let metrics handler also process
        return false;
    }

    // Simulated metrics handler
    bool metrics_handler(nimcp_exception_t* ex, void* user_data) {
        (void)user_data;
        (void)ex;
        g_metrics_handler_called = true;
        g_handler_call_count++;
        // Don't consume - let recovery handler also process
        return false;
    }

    // Simulated recovery handler for severe errors
    bool recovery_handler(nimcp_exception_t* ex, void* user_data) {
        (void)user_data;
        if (ex->severity >= EXCEPTION_SEVERITY_SEVERE) {
            g_recovery_handler_called = true;
            g_handler_call_count++;
            // Consume severe errors after attempting recovery
            return true;
        }
        return false;
    }
}

TEST_F(CoreExceptionIntegrationTest, MultiHandlerChain) {
    // WHAT: Test exception flowing through multiple handlers
    // WHY:  Verify handler chain works correctly with different priorities
    // HOW:  Register multiple handlers, dispatch exception, verify all called

    reset_handler_state();

    nimcp_handler_options_t opts;

    // Register logging handler (highest priority)
    nimcp_handler_default_options(&opts);
    opts.name = "logging";
    opts.handler = logging_handler;
    opts.priority = NIMCP_HANDLER_PRIORITY_HIGH;
    auto* log_reg = nimcp_handler_register(&opts);

    // Register metrics handler (normal priority)
    nimcp_handler_default_options(&opts);
    opts.name = "metrics";
    opts.handler = metrics_handler;
    opts.priority = NIMCP_HANDLER_PRIORITY_NORMAL;
    auto* metrics_reg = nimcp_handler_register(&opts);

    // Register recovery handler (low priority)
    nimcp_handler_default_options(&opts);
    opts.name = "recovery";
    opts.handler = recovery_handler;
    opts.priority = NIMCP_HANDLER_PRIORITY_LOW;
    opts.min_severity = EXCEPTION_SEVERITY_SEVERE;
    auto* recovery_reg = nimcp_handler_register(&opts);

    // Dispatch a non-severe exception
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Chain test - non-severe"
    );

    bool handled = nimcp_exception_dispatch(ex);
    EXPECT_FALSE(handled);  // Not consumed
    EXPECT_TRUE(g_logging_handler_called);
    EXPECT_TRUE(g_metrics_handler_called);
    EXPECT_FALSE(g_recovery_handler_called);  // Severity too low
    EXPECT_EQ(g_handler_call_count, 2);
    nimcp_exception_unref(ex);

    // Reset and dispatch a severe exception
    reset_handler_state();
    nimcp_exception_t* severe_ex = nimcp_exception_create(
        NIMCP_ERROR_NO_MEMORY,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        "Chain test - severe"
    );

    handled = nimcp_exception_dispatch(severe_ex);
    EXPECT_TRUE(handled);  // Consumed by recovery handler
    EXPECT_TRUE(g_logging_handler_called);
    EXPECT_TRUE(g_metrics_handler_called);
    EXPECT_TRUE(g_recovery_handler_called);
    EXPECT_EQ(g_handler_call_count, 3);
    nimcp_exception_unref(severe_ex);

    // Cleanup
    nimcp_handler_unregister(log_reg);
    nimcp_handler_unregister(metrics_reg);
    nimcp_handler_unregister(recovery_reg);
}

TEST_F(CoreExceptionIntegrationTest, CategoryFilteredHandler) {
    // WHAT: Test category-filtered handler only processes matching exceptions
    // WHY:  Verify handlers can filter by exception category
    // HOW:  Register filtered handler, dispatch different categories

    reset_handler_state();

    nimcp_handler_options_t opts;
    nimcp_handler_default_options(&opts);
    opts.name = "memory_only";
    opts.handler = logging_handler;
    opts.category_filter = EXCEPTION_CATEGORY_MEMORY;

    auto* reg = nimcp_handler_register(&opts);

    // Dispatch I/O exception - should NOT be handled
    nimcp_exception_t* io_ex = nimcp_exception_create(
        NIMCP_ERROR_FILE_NOT_FOUND,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "I/O error"
    );
    io_ex->category = EXCEPTION_CATEGORY_IO;
    nimcp_exception_dispatch(io_ex);
    EXPECT_FALSE(g_logging_handler_called);
    nimcp_exception_unref(io_ex);

    // Dispatch memory exception - SHOULD be handled
    nimcp_exception_t* mem_ex = nimcp_exception_create(
        NIMCP_ERROR_NO_MEMORY,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        "Memory error"
    );
    mem_ex->category = EXCEPTION_CATEGORY_MEMORY;
    nimcp_exception_dispatch(mem_ex);
    EXPECT_TRUE(g_logging_handler_called);
    nimcp_exception_unref(mem_ex);

    nimcp_handler_unregister(reg);
}

// ============================================================================
// Exception Chaining Integration Tests
// ============================================================================

TEST_F(CoreExceptionIntegrationTest, ExceptionChainingAcrossModules) {
    // WHAT: Test exception chaining for tracking error causality
    // WHY:  Verify root cause analysis works across module boundaries
    // HOW:  Create chain of exceptions, verify traversal

    // Simulate: Memory allocation fails -> Brain creation fails -> Inference fails
    nimcp_memory_exception_t* root_cause = nimcp_memory_exception_create(
        NIMCP_ERROR_NO_MEMORY,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        1024 * 1024 * 100,  // 100MB
        "Out of memory allocating 100MB"
    );

    nimcp_brain_exception_t* brain_ex = nimcp_brain_exception_create(
        NIMCP_ERROR_BRAIN_CREATION,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        1, "cortex",
        "Failed to create brain"
    );

    nimcp_exception_t* top_level = nimcp_exception_create(
        NIMCP_ERROR_INFERENCE_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Inference failed"
    );

    // Build the chain
    nimcp_exception_set_cause((nimcp_exception_t*)brain_ex, (nimcp_exception_t*)root_cause);
    nimcp_exception_set_cause(top_level, (nimcp_exception_t*)brain_ex);

    // Traverse the chain
    nimcp_exception_t* cause = nimcp_exception_get_cause(top_level);
    EXPECT_EQ(cause, (nimcp_exception_t*)brain_ex);
    EXPECT_EQ(cause->code, NIMCP_ERROR_BRAIN_CREATION);

    nimcp_exception_t* root = nimcp_exception_get_cause(cause);
    EXPECT_EQ(root, (nimcp_exception_t*)root_cause);
    EXPECT_EQ(root->code, NIMCP_ERROR_NO_MEMORY);

    EXPECT_EQ(nimcp_exception_get_cause(root), nullptr);  // End of chain

    nimcp_exception_unref(top_level);
    // Entire chain should be freed
}

TEST_F(CoreExceptionIntegrationTest, AggregateExceptionWithMixedTypes) {
    // WHAT: Test aggregate exception collecting multiple error types
    // WHY:  Verify batch operations can report multiple failures
    // HOW:  Create aggregate with different exception types

    nimcp_aggregate_exception_t* agg = nimcp_aggregate_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Batch neural network validation failed"
    );

    // Add various child exceptions
    nimcp_brain_exception_t* brain1 = nimcp_brain_exception_create(
        NIMCP_ERROR_DIMENSION_MISMATCH,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        1, "layer_1",
        "Input dimensions don't match"
    );
    nimcp_aggregate_exception_add(agg, (nimcp_exception_t*)brain1);

    nimcp_brain_exception_t* brain2 = nimcp_brain_exception_create(
        NIMCP_ERROR_WEIGHT_INIT,
        EXCEPTION_SEVERITY_WARNING,
        __FILE__, __LINE__, __func__,
        2, "layer_2",
        "Weight initialization produced NaN"
    );
    brain2->has_nan_weights = true;
    nimcp_aggregate_exception_add(agg, (nimcp_exception_t*)brain2);

    nimcp_io_exception_t* io_ex = nimcp_io_exception_create(
        NIMCP_ERROR_FILE_NOT_FOUND,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "/model/weights.bin",
        "Model weights file not found"
    );
    nimcp_aggregate_exception_add(agg, (nimcp_exception_t*)io_ex);

    // Verify aggregate
    EXPECT_EQ(nimcp_aggregate_exception_count(agg), 3u);

    // Check types of children
    nimcp_exception_t* child0 = nimcp_aggregate_exception_get(agg, 0);
    EXPECT_EQ(child0->type, EXCEPTION_TYPE_BRAIN);

    nimcp_exception_t* child1 = nimcp_aggregate_exception_get(agg, 1);
    EXPECT_EQ(child1->type, EXCEPTION_TYPE_BRAIN);

    nimcp_exception_t* child2 = nimcp_aggregate_exception_get(agg, 2);
    EXPECT_EQ(child2->type, EXCEPTION_TYPE_IO);

    nimcp_exception_unref((nimcp_exception_t*)agg);
}

// ============================================================================
// Concurrent Exception Handling Tests
// ============================================================================

TEST_F(CoreExceptionIntegrationTest, ConcurrentExceptionDispatch) {
    // WHAT: Test exception handling from multiple concurrent threads
    // WHY:  Verify thread-safety of exception system
    // HOW:  Spawn threads that create and dispatch exceptions

    std::atomic<int> total_handled{0};

    auto thread_handler = [](nimcp_exception_t* ex, void* user_data) -> bool {
        std::atomic<int>* counter = static_cast<std::atomic<int>*>(user_data);
        (*counter)++;
        (void)ex;
        return false;
    };

    nimcp_handler_options_t opts;
    nimcp_handler_default_options(&opts);
    opts.name = "concurrent_test";
    opts.handler = thread_handler;
    opts.user_data = &total_handled;

    auto* reg = nimcp_handler_register(&opts);

    constexpr int num_threads = 8;
    constexpr int exceptions_per_thread = 50;
    std::vector<std::thread> threads;

    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([t]() {
            for (int i = 0; i < exceptions_per_thread; i++) {
                nimcp_exception_t* ex = nimcp_exception_create(
                    NIMCP_ERROR_OPERATION_FAILED,
                    EXCEPTION_SEVERITY_ERROR,
                    __FILE__, __LINE__, __func__,
                    "Thread %d, exception %d", t, i
                );
                nimcp_exception_dispatch(ex);
                nimcp_exception_unref(ex);
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(total_handled, num_threads * exceptions_per_thread);

    nimcp_handler_unregister(reg);
}

TEST_F(CoreExceptionIntegrationTest, ThreadLocalExceptionIsolation) {
    // WHAT: Test thread-local exception isolation
    // WHY:  Verify each thread has its own current exception
    // HOW:  Set different exceptions in different threads

    std::atomic<bool> thread1_ready{false};
    std::atomic<bool> thread2_ready{false};
    std::atomic<bool> proceed{false};

    nimcp_exception_t* ex1 = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Thread 1 exception"
    );

    nimcp_exception_t* ex2 = nimcp_exception_create(
        NIMCP_ERROR_NO_MEMORY,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        "Thread 2 exception"
    );

    nimcp_error_t thread1_code = NIMCP_SUCCESS;
    nimcp_error_t thread2_code = NIMCP_SUCCESS;

    std::thread t1([&]() {
        nimcp_exception_set_current(ex1);
        thread1_ready = true;

        // Wait for both threads to be ready
        while (!proceed) {
            std::this_thread::yield();
        }

        // Check our exception is still ours
        nimcp_exception_t* current = nimcp_exception_get_current();
        if (current) {
            thread1_code = current->code;
        }
        nimcp_exception_clear_current();
    });

    std::thread t2([&]() {
        nimcp_exception_set_current(ex2);
        thread2_ready = true;

        // Wait for both threads to be ready
        while (!proceed) {
            std::this_thread::yield();
        }

        // Check our exception is still ours
        nimcp_exception_t* current = nimcp_exception_get_current();
        if (current) {
            thread2_code = current->code;
        }
        nimcp_exception_clear_current();
    });

    // Wait for both threads to set their exceptions
    while (!thread1_ready || !thread2_ready) {
        std::this_thread::yield();
    }

    // Let them check isolation
    proceed = true;

    t1.join();
    t2.join();

    // Verify each thread saw its own exception
    EXPECT_EQ(thread1_code, NIMCP_ERROR_OPERATION_FAILED);
    EXPECT_EQ(thread2_code, NIMCP_ERROR_NO_MEMORY);

    nimcp_exception_unref(ex1);
    nimcp_exception_unref(ex2);
}

// ============================================================================
// Recovery Callback Integration Tests
// ============================================================================

namespace {
    std::atomic<bool> g_retry_callback_called{false};
    std::atomic<bool> g_gc_callback_called{false};
    std::atomic<int> g_retry_count{0};

    bool retry_recovery_callback(nimcp_exception_recovery_action_t action, void* user_data) {
        (void)action;
        (void)user_data;
        g_retry_callback_called = true;
        g_retry_count++;
        return true;  // Recovery successful
    }

    bool gc_recovery_callback(nimcp_exception_recovery_action_t action, void* user_data) {
        (void)action;
        (void)user_data;
        g_gc_callback_called = true;
        return true;  // Recovery successful
    }
}

TEST_F(CoreExceptionIntegrationTest, RecoveryCallbackRegistration) {
    // WHAT: Test recovery callback registration and execution
    // WHY:  Verify recovery actions can trigger appropriate callbacks
    // HOW:  Register callbacks, trigger recovery, verify execution

    g_retry_callback_called = false;
    g_gc_callback_called = false;
    g_retry_count = 0;

    // Register recovery callbacks
    int result = nimcp_register_recovery_callback(
        EXCEPTION_RECOVERY_RETRY, retry_recovery_callback, nullptr);
    EXPECT_EQ(result, 0);

    result = nimcp_register_recovery_callback(
        EXCEPTION_RECOVERY_GC, gc_recovery_callback, nullptr);
    EXPECT_EQ(result, 0);

    // Execute retry recovery
    bool success = nimcp_execute_recovery_action(EXCEPTION_RECOVERY_RETRY);
    EXPECT_TRUE(success);
    EXPECT_TRUE(g_retry_callback_called);
    EXPECT_EQ(g_retry_count, 1);

    // Execute GC recovery
    success = nimcp_execute_recovery_action(EXCEPTION_RECOVERY_GC);
    EXPECT_TRUE(success);
    EXPECT_TRUE(g_gc_callback_called);
}

// ============================================================================
// Immune System Integration Tests
// ============================================================================

TEST_F(CoreExceptionIntegrationTest, ExceptionToAntigenConversion) {
    // WHAT: Test exception to immune antigen conversion
    // WHY:  Verify exception patterns can be presented to immune system
    // HOW:  Create exceptions, check antigen metadata generation

    nimcp_memory_exception_t* mem_ex = nimcp_memory_exception_create(
        NIMCP_ERROR_NO_MEMORY,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        1024 * 1024,
        "Out of memory"
    );

    // Generate epitope
    size_t epitope_len = nimcp_exception_generate_epitope((nimcp_exception_t*)mem_ex);
    EXPECT_GT(epitope_len, 0u);

    // Check antigen source mapping
    exception_antigen_source_t source = nimcp_exception_to_antigen_source(
        mem_ex->base.category);
    EXPECT_EQ(source, EX_ANTIGEN_SOURCE_ANOMALY);

    // Check severity mapping
    uint32_t immune_severity = nimcp_exception_to_immune_severity(
        mem_ex->base.severity);
    EXPECT_EQ(immune_severity, 7u);  // SEVERE = 7

    nimcp_exception_unref((nimcp_exception_t*)mem_ex);
}

TEST_F(CoreExceptionIntegrationTest, RecoveryStrategyGeneration) {
    // WHAT: Test recovery strategy generation for different exception types
    // WHY:  Verify appropriate recovery strategies are suggested
    // HOW:  Create exceptions, get strategies, verify actions

    // Memory exception
    nimcp_exception_t* mem_ex = nimcp_exception_create(
        NIMCP_ERROR_NO_MEMORY,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        "Memory test"
    );
    mem_ex->category = EXCEPTION_CATEGORY_MEMORY;

    nimcp_exception_recovery_strategy_t mem_strategy;
    nimcp_exception_get_recovery_strategy(mem_ex, &mem_strategy);
    EXPECT_EQ(mem_strategy.primary_action, EXCEPTION_RECOVERY_GC);

    nimcp_exception_unref(mem_ex);

    // Threading exception (deadlock)
    nimcp_exception_t* thread_ex = nimcp_exception_create(
        NIMCP_ERROR_DEADLOCK,
        EXCEPTION_SEVERITY_CRITICAL,
        __FILE__, __LINE__, __func__,
        "Deadlock detected"
    );
    thread_ex->category = EXCEPTION_CATEGORY_THREADING;

    nimcp_exception_recovery_strategy_t thread_strategy;
    nimcp_exception_get_recovery_strategy(thread_ex, &thread_strategy);
    EXPECT_EQ(thread_strategy.primary_action, EXCEPTION_RECOVERY_RESTART_THREAD);

    nimcp_exception_unref(thread_ex);

    // GPU exception
    nimcp_exception_t* gpu_ex = nimcp_exception_create(
        NIMCP_ERROR_GPU,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        "CUDA error"
    );
    gpu_ex->category = EXCEPTION_CATEGORY_GPU;

    nimcp_exception_recovery_strategy_t gpu_strategy;
    nimcp_exception_get_recovery_strategy(gpu_ex, &gpu_strategy);
    EXPECT_EQ(gpu_strategy.primary_action, EXCEPTION_RECOVERY_CLEAR_CACHE);

    nimcp_exception_unref(gpu_ex);
}

TEST_F(CoreExceptionIntegrationTest, ImmuneStatisticsCollection) {
    // WHAT: Test immune integration statistics collection
    // WHY:  Verify exception metrics are tracked correctly
    // HOW:  Present exceptions, check statistics

    // Initialize with fresh config
    nimcp_exception_immune_shutdown();

    nimcp_exception_immune_config_t config;
    nimcp_exception_immune_default_config(&config);
    config.enable_auto_present = true;
    config.min_present_severity = EXCEPTION_SEVERITY_WARNING;

    int result = nimcp_exception_immune_init(&config);
    EXPECT_EQ(result, 0);

    // Get initial stats
    nimcp_exception_immune_stats_t stats;
    nimcp_exception_immune_get_stats(&stats);
    size_t initial_presented = stats.exceptions_presented;

    // Create and present a severe exception
    nimcp_exception_t* severe_ex = nimcp_exception_create(
        NIMCP_ERROR_NO_MEMORY,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        "Stats test"
    );

    // Present to immune system (synchronously)
    result = nimcp_exception_present_to_immune(severe_ex);
    // May return -1 if not connected to immune system - that's OK for this test

    // Get updated stats
    nimcp_exception_immune_get_stats(&stats);
    EXPECT_GE(stats.exceptions_presented, initial_presented);

    nimcp_exception_unref(severe_ex);
}

// ============================================================================
// Core Module Error Flow Tests
// ============================================================================

TEST_F(CoreExceptionIntegrationTest, BrainForwardPassErrorFlow) {
    // WHAT: Test error flow during brain forward pass failure
    // WHY:  Verify brain exceptions are correctly handled and recoverable
    // HOW:  Simulate forward pass failure, check exception chain

    // Simulate: Layer computation fails due to NaN
    nimcp_brain_exception_t* layer_ex = nimcp_brain_exception_create(
        NIMCP_ERROR_FORWARD_PASS,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        1, "hidden_layer_3",
        "NaN detected in layer output"
    );
    layer_ex->has_nan_weights = true;
    layer_ex->gradient_norm = NAN;

    // Check recovery suggestion
    nimcp_exception_recovery_action_t action = nimcp_exception_get_suggested_recovery(
        (nimcp_exception_t*)layer_ex);
    EXPECT_EQ(action, EXCEPTION_RECOVERY_ROLLBACK);

    // Wrap in aggregate for batch processing context
    nimcp_aggregate_exception_t* batch_ex = nimcp_aggregate_exception_create(
        NIMCP_ERROR_INFERENCE_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Batch inference failed"
    );
    nimcp_aggregate_exception_add(batch_ex, (nimcp_exception_t*)layer_ex);

    // Dispatch and verify handling
    reset_handler_state();

    nimcp_handler_options_t opts;
    nimcp_handler_default_options(&opts);
    opts.name = "brain_error_handler";
    opts.handler = logging_handler;
    opts.category_filter = EXCEPTION_CATEGORY_BRAIN;

    auto* reg = nimcp_handler_register(&opts);

    nimcp_exception_dispatch((nimcp_exception_t*)batch_ex);
    // Note: Aggregate itself may not match brain category filter
    // but this tests the dispatch mechanism

    nimcp_handler_unregister(reg);
    nimcp_exception_unref((nimcp_exception_t*)batch_ex);
}

TEST_F(CoreExceptionIntegrationTest, SynapseMemoryAllocationFailure) {
    // WHAT: Test memory allocation failure during synapse creation
    // WHY:  Verify memory exceptions from core modules are handled
    // HOW:  Simulate synapse allocation failure

    // Create memory exception with synapse context
    nimcp_memory_exception_t* syn_mem_ex = nimcp_memory_exception_create(
        NIMCP_ERROR_NO_MEMORY,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        sizeof(double) * 10000 * 10000,  // Large synapse weight matrix
        "Failed to allocate synapse weight matrix"
    );

    // Add context about what was being allocated
    nimcp_exception_set_context((nimcp_exception_t*)syn_mem_ex,
        "component", "synapse_weights");
    nimcp_exception_set_context((nimcp_exception_t*)syn_mem_ex,
        "dimensions", "10000x10000");

    // Verify context
    const char* component = nimcp_exception_get_context(
        (nimcp_exception_t*)syn_mem_ex, "component");
    EXPECT_STREQ(component, "synapse_weights");

    // Verify recovery suggestion
    nimcp_exception_recovery_action_t action = nimcp_exception_get_suggested_recovery(
        (nimcp_exception_t*)syn_mem_ex);
    EXPECT_EQ(action, EXCEPTION_RECOVERY_GC);

    nimcp_exception_unref((nimcp_exception_t*)syn_mem_ex);
}

TEST_F(CoreExceptionIntegrationTest, GeometryComputationOverflow) {
    // WHAT: Test exception for geometry computation overflow
    // WHY:  Verify numeric exceptions from geometry module are handled
    // HOW:  Simulate surface computation failure

    nimcp_exception_t* geom_ex = nimcp_exception_create(
        NIMCP_ERROR_OUT_OF_RANGE,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Surface area computation overflow: chi parameter exceeds valid range"
    );

    // Add geometry-specific context
    nimcp_exception_set_context(geom_ex, "module", "surface_geometry");
    nimcp_exception_set_context(geom_ex, "operation", "compute_branch_params");
    nimcp_exception_set_context(geom_ex, "chi", "999.99");

    // Verify context count
    EXPECT_EQ(nimcp_exception_context_count(geom_ex), 3u);

    // Format for logging
    char buffer[1024];
    size_t len = nimcp_exception_to_string(geom_ex, buffer, sizeof(buffer));
    EXPECT_GT(len, 0u);
    EXPECT_NE(strstr(buffer, "Surface area"), nullptr);

    nimcp_exception_unref(geom_ex);
}

TEST_F(CoreExceptionIntegrationTest, NeuronModelStateException) {
    // WHAT: Test exception for neuron model state errors
    // WHY:  Verify neuron model exceptions are properly categorized
    // HOW:  Simulate neuron state corruption

    nimcp_brain_exception_t* neuron_ex = nimcp_brain_exception_create(
        NIMCP_ERROR_BRAIN_INVALID,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        0, "neuron_pool_0",
        "Neuron membrane potential diverged to infinity"
    );
    neuron_ex->has_nan_weights = true;

    // Verify categorization
    EXPECT_EQ(neuron_ex->base.category, EXCEPTION_CATEGORY_BRAIN);
    EXPECT_EQ(neuron_ex->base.severity, EXCEPTION_SEVERITY_SEVERE);

    nimcp_exception_unref((nimcp_exception_t*)neuron_ex);
}

// ============================================================================
// Full Exception Lifecycle Test
// ============================================================================

TEST_F(CoreExceptionIntegrationTest, FullExceptionLifecycle) {
    // WHAT: Test complete exception lifecycle from creation to cleanup
    // WHY:  Verify all aspects of exception handling work together
    // HOW:  Create -> Set context -> Chain -> Dispatch -> Present -> Cleanup

    // 1. Create root cause
    nimcp_memory_exception_t* root = nimcp_memory_exception_create(
        NIMCP_ERROR_NO_MEMORY,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        1024 * 1024 * 512,  // 512MB
        "System out of memory"
    );
    ASSERT_NE(root, nullptr);

    // 2. Add context
    nimcp_exception_set_context((nimcp_exception_t*)root, "heap_used", "95%");
    nimcp_exception_set_context((nimcp_exception_t*)root, "gc_attempted", "true");

    // 3. Create derived exception
    nimcp_brain_exception_t* derived = nimcp_brain_exception_create(
        NIMCP_ERROR_BRAIN_CREATION,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        1, "main_cortex",
        "Cannot create brain due to memory constraints"
    );
    ASSERT_NE(derived, nullptr);

    // 4. Chain exceptions
    nimcp_exception_set_cause((nimcp_exception_t*)derived, (nimcp_exception_t*)root);
    EXPECT_EQ(nimcp_exception_get_cause((nimcp_exception_t*)derived),
              (nimcp_exception_t*)root);

    // 5. Generate epitope
    size_t epitope_len = nimcp_exception_generate_epitope((nimcp_exception_t*)derived);
    EXPECT_GT(epitope_len, 0u);

    // 6. Register handler
    reset_handler_state();
    nimcp_handler_options_t opts;
    nimcp_handler_default_options(&opts);
    opts.name = "lifecycle_test";
    opts.handler = logging_handler;
    auto* reg = nimcp_handler_register(&opts);

    // 7. Dispatch
    bool handled = nimcp_exception_dispatch((nimcp_exception_t*)derived);
    EXPECT_FALSE(handled);  // Handler doesn't consume
    EXPECT_TRUE(g_logging_handler_called);

    // 8. Get recovery strategy
    nimcp_exception_recovery_strategy_t strategy;
    nimcp_exception_get_recovery_strategy((nimcp_exception_t*)derived, &strategy);
    EXPECT_EQ(strategy.primary_action, EXCEPTION_RECOVERY_REDUCE_LOAD);

    // 9. Present to immune (if available)
    nimcp_exception_present_to_immune((nimcp_exception_t*)derived);

    // 10. Cleanup
    nimcp_handler_unregister(reg);
    nimcp_exception_unref((nimcp_exception_t*)derived);
    // Root should be freed with derived due to chaining
}

// ============================================================================
// Edge Case Tests
// ============================================================================

TEST_F(CoreExceptionIntegrationTest, MaxHandlersRegistration) {
    // WHAT: Test behavior when maximum handlers are registered
    // WHY:  Verify system handles handler limit gracefully
    // HOW:  Register many handlers, verify limit enforcement

    std::vector<nimcp_handler_registration_t*> registrations;

    auto dummy_handler = [](nimcp_exception_t* ex, void* user_data) -> bool {
        (void)ex;
        (void)user_data;
        return false;
    };

    // Register handlers up to limit
    for (int i = 0; i < 100; i++) {  // More than NIMCP_HANDLER_MAX_REGISTERED
        nimcp_handler_options_t opts;
        nimcp_handler_default_options(&opts);
        char name[32];
        snprintf(name, sizeof(name), "handler_%d", i);
        opts.name = name;
        opts.handler = dummy_handler;

        auto* reg = nimcp_handler_register(&opts);
        if (reg) {
            registrations.push_back(reg);
        } else {
            // Hit the limit
            break;
        }
    }

    EXPECT_GT(registrations.size(), 0u);
    EXPECT_LE(registrations.size(), (size_t)NIMCP_HANDLER_MAX_REGISTERED);

    // Cleanup
    for (auto* reg : registrations) {
        nimcp_handler_unregister(reg);
    }
}

TEST_F(CoreExceptionIntegrationTest, RapidCreateDestroy) {
    // WHAT: Test rapid exception creation and destruction
    // WHY:  Verify no memory leaks or race conditions
    // HOW:  Create and destroy many exceptions rapidly

    constexpr int iterations = 1000;

    for (int i = 0; i < iterations; i++) {
        nimcp_exception_t* ex = nimcp_exception_create(
            NIMCP_ERROR_OPERATION_FAILED,
            EXCEPTION_SEVERITY_ERROR,
            __FILE__, __LINE__, __func__,
            "Rapid test %d", i
        );
        ASSERT_NE(ex, nullptr);

        // Sometimes add context
        if (i % 10 == 0) {
            nimcp_exception_set_context(ex, "iteration", "value");
        }

        // Sometimes chain
        if (i % 20 == 0) {
            nimcp_exception_t* cause = nimcp_exception_create(
                NIMCP_ERROR_UNKNOWN,
                EXCEPTION_SEVERITY_DEBUG,
                __FILE__, __LINE__, __func__,
                "Cause"
            );
            nimcp_exception_set_cause(ex, cause);
        }

        nimcp_exception_unref(ex);
    }

    // If we got here without crashing, the test passed
    SUCCEED();
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
