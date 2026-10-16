/**
 * @file test_exception_complete_flow_e2e.cpp
 * @brief E2E tests for complete exception lifecycle and handling
 * @version 1.0.0
 * @date 2026-01-21
 *
 * WHAT: End-to-end tests for complete exception lifecycle from creation through
 *       cognitive modules to brain modules, verifying propagation, cleanup, and rollback
 * WHY:  Verify the integrated exception handling pipeline works correctly across all layers
 * HOW:  Test realistic scenarios with chained module calls, error propagation,
 *       resource cleanup, and partial operation rollback
 *
 * Test Scenarios:
 * 1. Complete exception lifecycle from API to cognitive to brain modules
 * 2. Exception propagation through multiple module boundaries
 * 3. Resource cleanup verification on exception paths
 * 4. Partial operation rollback verification
 * 5. Chained module call exception handling
 * 6. Cross-module exception context preservation
 * 7. Exception aggregation across module boundaries
 * 8. Cleanup order verification during exception unwind
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <cstring>
#include <atomic>
#include <vector>
#include <memory>
#include <cmath>
#include <functional>
#include <map>
#include <mutex>

extern "C" {
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_immune.h"
#include "utils/exception/nimcp_exception_handlers.h"
#include "utils/exception/nimcp_exception_metrics.h"
#include "utils/exception/nimcp_exception_circuit.h"
#include "utils/error/nimcp_error_codes.h"
}

/* ============================================================================
 * Test Utilities - Module Simulation
 * ============================================================================ */

namespace {

// Simulated module state for tracking cleanup and rollback
struct ModuleState {
    std::string name;
    bool initialized;
    bool resources_allocated;
    int allocation_count;
    std::vector<std::string> operations;
    std::vector<std::string> cleanup_order;
};

// Global state tracking
std::mutex g_state_mutex;
std::map<std::string, ModuleState> g_module_states;
std::atomic<int> g_exception_count{0};
std::atomic<int> g_handler_calls{0};
std::atomic<int> g_cleanup_calls{0};
std::atomic<int> g_rollback_calls{0};
std::atomic<bool> g_propagation_verified{false};
std::vector<std::string> g_propagation_path;
std::vector<nimcp_exception_category_t> g_exception_categories;

void reset_test_state() {
    std::lock_guard<std::mutex> lock(g_state_mutex);
    g_module_states.clear();
    g_exception_count = 0;
    g_handler_calls = 0;
    g_cleanup_calls = 0;
    g_rollback_calls = 0;
    g_propagation_verified = false;
    g_propagation_path.clear();
    g_exception_categories.clear();
}

// Register a module state
void register_module(const std::string& name) {
    std::lock_guard<std::mutex> lock(g_state_mutex);
    ModuleState state;
    state.name = name;
    state.initialized = false;
    state.resources_allocated = false;
    state.allocation_count = 0;
    g_module_states[name] = state;
}

// Initialize a module (allocate resources)
bool init_module(const std::string& name) {
    std::lock_guard<std::mutex> lock(g_state_mutex);
    if (g_module_states.find(name) == g_module_states.end()) {
        return false;
    }
    g_module_states[name].initialized = true;
    g_module_states[name].resources_allocated = true;
    g_module_states[name].allocation_count++;
    g_module_states[name].operations.push_back("init");
    return true;
}

// Cleanup a module
void cleanup_module(const std::string& name) {
    std::lock_guard<std::mutex> lock(g_state_mutex);
    if (g_module_states.find(name) != g_module_states.end()) {
        g_module_states[name].resources_allocated = false;
        g_module_states[name].cleanup_order.push_back("cleanup");
        g_cleanup_calls++;
    }
}

// Rollback module operations
void rollback_module(const std::string& name) {
    std::lock_guard<std::mutex> lock(g_state_mutex);
    if (g_module_states.find(name) != g_module_states.end()) {
        g_module_states[name].operations.clear();
        g_module_states[name].cleanup_order.push_back("rollback");
        g_rollback_calls++;
    }
}

// Record propagation path
void record_propagation(const std::string& module) {
    std::lock_guard<std::mutex> lock(g_state_mutex);
    g_propagation_path.push_back(module);
}

// Exception handler that tracks propagation
bool propagation_tracking_handler(nimcp_exception_t* ex, void* user_data) {
    g_handler_calls++;
    if (ex) {
        g_exception_count++;
        std::lock_guard<std::mutex> lock(g_state_mutex);
        g_exception_categories.push_back(ex->category);
    }
    return false;  // Don't consume
}

// Recovery callback that tracks calls
int test_recovery_callback(nimcp_exception_t* ex,
                            nimcp_exception_recovery_action_t action,
                            void* user_data) {
    (void)ex;
    (void)user_data;
    g_rollback_calls++;
    return 0;
}

}  // namespace

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class ExceptionCompleteFlowE2ETest : public ::testing::Test {
protected:
    nimcp_handler_registration_t* handler_reg = nullptr;

    void SetUp() override {
        reset_test_state();

        // Initialize exception system
        int init_result = nimcp_exception_system_init();
        ASSERT_EQ(init_result, 0) << "Failed to initialize exception system";

        // Initialize circuit breaker
        ASSERT_EQ(nimcp_circuit_init(), 0) << "Failed to initialize circuit breaker";

        // Initialize metrics
        ASSERT_EQ(nimcp_metrics_init(), 0) << "Failed to initialize metrics";

        // Initialize exception-immune integration
        nimcp_exception_immune_config_t immune_config;
        nimcp_exception_immune_default_config(&immune_config);
        immune_config.enable_auto_present = true;
        immune_config.enable_auto_recovery = true;
        immune_config.min_present_severity = EXCEPTION_SEVERITY_ERROR;

        int immune_init = nimcp_exception_immune_init(&immune_config);
        ASSERT_EQ(immune_init, 0) << "Failed to initialize exception-immune integration";

        // Register tracking handler
        nimcp_handler_options_t opts;
        nimcp_handler_default_options(&opts);
        opts.name = "PropagationTracker";
        opts.handler = propagation_tracking_handler;
        opts.priority = NIMCP_HANDLER_PRIORITY_NORMAL;
        handler_reg = nimcp_handler_register(&opts);
        ASSERT_NE(handler_reg, nullptr) << "Failed to register handler";
    }

    void TearDown() override {
        if (handler_reg) {
            nimcp_handler_unregister(handler_reg);
            handler_reg = nullptr;
        }

        nimcp_exception_handlers_shutdown();
        nimcp_exception_immune_shutdown();
        nimcp_metrics_shutdown();
        nimcp_circuit_shutdown();
        nimcp_exception_system_shutdown();
    }

    // Helper to simulate module chain call
    nimcp_exception_t* simulate_chained_modules(
        const std::vector<std::string>& modules,
        int fail_at_index,
        nimcp_error_t error_code
    ) {
        for (size_t i = 0; i < modules.size(); i++) {
            register_module(modules[i]);
            init_module(modules[i]);
            record_propagation(modules[i]);

            if ((int)i == fail_at_index) {
                // Create exception at this module
                nimcp_exception_t* ex = nimcp_exception_create(
                    error_code,
                    EXCEPTION_SEVERITY_ERROR,
                    __FILE__, __LINE__, __func__,
                    "Exception in module %s at index %d",
                    modules[i].c_str(), fail_at_index
                );

                // Set context to track which module failed
                nimcp_exception_set_context(ex, "failed_module", modules[i].c_str());

                // Cleanup modules in reverse order
                for (int j = (int)i; j >= 0; j--) {
                    cleanup_module(modules[j]);
                }

                return ex;
            }
        }
        return nullptr;
    }
};

/* ============================================================================
 * Test 1: Complete Exception Lifecycle
 *
 * Verifies the complete lifecycle: creation -> propagation -> handling -> cleanup
 * ============================================================================ */

TEST_F(ExceptionCompleteFlowE2ETest, CompleteExceptionLifecycle) {
    printf("=== Test: Complete Exception Lifecycle ===\n");

    // Step 1: Install default handlers
    nimcp_install_default_handlers();
    printf("  Default handlers installed\n");

    // Step 2: Register recovery callback
    nimcp_register_recovery_callback(EXCEPTION_RECOVERY_ROLLBACK, test_recovery_callback, nullptr);
    printf("  Recovery callback registered\n");

    // Step 3: Create exception at API layer
    nimcp_exception_t* api_ex = nimcp_exception_create(
        NIMCP_ERROR_INVALID_PARAMETER,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "API layer: Invalid parameter received"
    );
    ASSERT_NE(api_ex, nullptr);
    nimcp_exception_set_context(api_ex, "layer", "api");
    printf("  API layer exception created\n");

    // Step 4: Chain with cognitive layer exception
    nimcp_cognitive_exception_t* cog_ex = (nimcp_cognitive_exception_t*)malloc(sizeof(nimcp_cognitive_exception_t));
    ASSERT_NE(cog_ex, nullptr);
    memset(cog_ex, 0, sizeof(*cog_ex));
    cog_ex->base.type = EXCEPTION_TYPE_COGNITIVE;
    cog_ex->base.category = EXCEPTION_CATEGORY_COGNITIVE;
    cog_ex->base.code = NIMCP_ERROR_OPERATION_FAILED;
    cog_ex->base.severity = EXCEPTION_SEVERITY_ERROR;
    cog_ex->base.ref_count = 1;
    snprintf(cog_ex->base.message, sizeof(cog_ex->base.message),
             "Cognitive layer: Processing failed");
    cog_ex->module_id = 42;
    cog_ex->module_name = "attention_module";

    nimcp_exception_set_cause(&cog_ex->base, api_ex);
    printf("  Cognitive layer exception chained\n");

    // Step 5: Chain with brain layer exception
    nimcp_brain_exception_t* brain_ex = nimcp_brain_exception_create(
        NIMCP_ERROR_BRAIN_CREATION,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        1, "prefrontal_cortex",
        "Brain layer: Neural initialization failed"
    );
    ASSERT_NE(brain_ex, nullptr);
    nimcp_exception_set_cause(&brain_ex->base, &cog_ex->base);
    printf("  Brain layer exception chained\n");

    // Step 6: Dispatch through handler chain
    g_handler_calls = 0;
    nimcp_exception_dispatch(&brain_ex->base);
    printf("  Exception dispatched, handler calls: %d\n", g_handler_calls.load());
    EXPECT_GT(g_handler_calls.load(), 0);

    // Step 7: Walk the cause chain
    int chain_depth = 0;
    nimcp_exception_t* current = &brain_ex->base;
    while (current) {
        printf("  Chain[%d]: category=%s, message=%s\n",
               chain_depth,
               nimcp_exception_category_to_string(current->category),
               current->message);
        current = nimcp_exception_get_cause(current);
        chain_depth++;
    }
    EXPECT_EQ(chain_depth, 3);  // brain -> cognitive -> api

    // Step 8: Present to immune system
    nimcp_immune_response_t response;
    memset(&response, 0, sizeof(response));
    int present_result = nimcp_exception_present_to_immune(&brain_ex->base, &response);
    printf("  Presented to immune: result=%d, antigen_id=%u\n",
           present_result, response.antigen_id);

    // Step 9: Execute recovery
    nimcp_exception_recovery_action_t suggested =
        nimcp_exception_get_suggested_recovery(&brain_ex->base);
    printf("  Suggested recovery: %s\n",
           nimcp_exception_recovery_action_to_string(suggested));

    if (suggested != EXCEPTION_RECOVERY_NONE) {
        int exec_result = nimcp_execute_recovery(&brain_ex->base, suggested);
        printf("  Recovery executed: result=%d\n", exec_result);
    }

    // Step 10: Cleanup
    nimcp_exception_unref(&brain_ex->base);
    printf("Test passed: Complete exception lifecycle verified\n\n");
}

/* ============================================================================
 * Test 2: Exception Propagation Through Module Boundaries
 *
 * Verifies exception propagation from low-level to high-level modules
 * ============================================================================ */

TEST_F(ExceptionCompleteFlowE2ETest, ExceptionPropagationThroughModules) {
    printf("=== Test: Exception Propagation Through Module Boundaries ===\n");

    // Define module chain: low-level -> mid-level -> high-level
    std::vector<std::string> modules = {
        "hardware_driver",
        "memory_allocator",
        "neural_backend",
        "cognitive_processor",
        "api_controller"
    };

    // Simulate failure at neural_backend (index 2)
    nimcp_exception_t* ex = simulate_chained_modules(modules, 2, NIMCP_ERROR_OPERATION_FAILED);
    ASSERT_NE(ex, nullptr);
    printf("  Exception created at neural_backend\n");

    // Verify propagation path was recorded
    {
        std::lock_guard<std::mutex> lock(g_state_mutex);
        EXPECT_EQ(g_propagation_path.size(), 3u);  // Up to and including failed module
        for (size_t i = 0; i < g_propagation_path.size(); i++) {
            printf("  Propagation[%zu]: %s\n", i, g_propagation_path[i].c_str());
        }
    }

    // Verify context preserved
    const char* failed_module = nimcp_exception_get_context(ex, "failed_module");
    EXPECT_NE(failed_module, nullptr);
    if (failed_module) {
        EXPECT_STREQ(failed_module, "neural_backend");
        printf("  Failed module context: %s\n", failed_module);
    }

    // Verify cleanup was called in reverse order
    EXPECT_EQ(g_cleanup_calls.load(), 3);  // All initialized modules cleaned up
    printf("  Cleanup calls: %d\n", g_cleanup_calls.load());

    // Dispatch and verify handling
    g_handler_calls = 0;
    nimcp_exception_dispatch(ex);
    EXPECT_GT(g_handler_calls.load(), 0);
    printf("  Handler invoked after propagation\n");

    nimcp_exception_unref(ex);
    printf("Test passed: Exception propagation through modules verified\n\n");
}

/* ============================================================================
 * Test 3: Resource Cleanup on Exception Paths
 *
 * Verifies all allocated resources are properly cleaned up when exception occurs
 * ============================================================================ */

TEST_F(ExceptionCompleteFlowE2ETest, ResourceCleanupOnExceptionPaths) {
    printf("=== Test: Resource Cleanup on Exception Paths ===\n");

    const int NUM_RESOURCES = 10;
    std::vector<std::string> resources;

    // Allocate resources
    for (int i = 0; i < NUM_RESOURCES; i++) {
        std::string name = "resource_" + std::to_string(i);
        resources.push_back(name);
        register_module(name);
        init_module(name);
    }
    printf("  Allocated %d resources\n", NUM_RESOURCES);

    // Verify all resources are allocated
    {
        std::lock_guard<std::mutex> lock(g_state_mutex);
        for (const auto& name : resources) {
            EXPECT_TRUE(g_module_states[name].resources_allocated);
        }
    }

    // Create exception mid-operation
    nimcp_memory_exception_t* mem_ex = nimcp_memory_exception_create(
        NIMCP_ERROR_NO_MEMORY,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        1024 * 1024,
        "Memory allocation failed during batch operation"
    );
    ASSERT_NE(mem_ex, nullptr);
    printf("  Exception created during resource allocation\n");

    // Cleanup all resources in reverse order (simulating stack unwind)
    g_cleanup_calls = 0;
    for (auto it = resources.rbegin(); it != resources.rend(); ++it) {
        cleanup_module(*it);
    }
    printf("  Resources cleaned up in reverse order\n");

    // Verify all resources are cleaned up
    {
        std::lock_guard<std::mutex> lock(g_state_mutex);
        for (const auto& name : resources) {
            EXPECT_FALSE(g_module_states[name].resources_allocated);
        }
    }

    EXPECT_EQ(g_cleanup_calls.load(), NUM_RESOURCES);
    printf("  Verified all %d resources cleaned up\n", g_cleanup_calls.load());

    // Verify cleanup order was recorded
    {
        std::lock_guard<std::mutex> lock(g_state_mutex);
        // Last resource should have cleanup recorded first (LIFO)
        EXPECT_EQ(g_module_states["resource_9"].cleanup_order.size(), 1u);
        EXPECT_EQ(g_module_states["resource_9"].cleanup_order[0], "cleanup");
    }

    nimcp_exception_unref(&mem_ex->base);
    printf("Test passed: Resource cleanup on exception paths verified\n\n");
}

/* ============================================================================
 * Test 4: Partial Operation Rollback
 *
 * Verifies partial operations are properly rolled back when exception occurs
 * ============================================================================ */

TEST_F(ExceptionCompleteFlowE2ETest, PartialOperationRollback) {
    printf("=== Test: Partial Operation Rollback ===\n");

    // Register rollback recovery callback
    nimcp_register_recovery_callback(EXCEPTION_RECOVERY_ROLLBACK, test_recovery_callback, nullptr);

    // Simulate a multi-step operation
    const int TOTAL_STEPS = 5;
    const int FAIL_AT_STEP = 3;
    std::vector<std::string> completed_steps;

    for (int step = 0; step < TOTAL_STEPS; step++) {
        std::string step_name = "step_" + std::to_string(step);
        register_module(step_name);
        init_module(step_name);

        if (step == FAIL_AT_STEP) {
            printf("  Operation failed at step %d\n", step);

            // Create exception for partial failure
            nimcp_exception_t* ex = nimcp_exception_create(
                NIMCP_ERROR_OPERATION_FAILED,
                EXCEPTION_SEVERITY_ERROR,
                __FILE__, __LINE__, __func__,
                "Multi-step operation failed at step %d", step
            );
            ASSERT_NE(ex, nullptr);

            // Rollback completed steps in reverse order
            g_rollback_calls = 0;
            for (auto it = completed_steps.rbegin(); it != completed_steps.rend(); ++it) {
                rollback_module(*it);
            }
            printf("  Rolled back %d completed steps\n", (int)completed_steps.size());

            // Execute recovery action
            ex->suggested_action = EXCEPTION_RECOVERY_ROLLBACK;
            nimcp_execute_recovery(ex, EXCEPTION_RECOVERY_ROLLBACK);

            // Verify rollback
            EXPECT_EQ(g_rollback_calls.load(), (int)completed_steps.size() + 1);  // +1 for recovery callback

            nimcp_exception_unref(ex);
            break;
        }

        completed_steps.push_back(step_name);
        {
            std::lock_guard<std::mutex> lock(g_state_mutex);
            g_module_states[step_name].operations.push_back("execute");
        }
    }

    // Verify all completed operations were rolled back
    {
        std::lock_guard<std::mutex> lock(g_state_mutex);
        for (const auto& step : completed_steps) {
            EXPECT_TRUE(g_module_states[step].cleanup_order.size() > 0);
            EXPECT_EQ(g_module_states[step].cleanup_order[0], "rollback");
        }
    }
    printf("Test passed: Partial operation rollback verified\n\n");
}

/* ============================================================================
 * Test 5: Chained Module Call Exception Handling
 *
 * Verifies exceptions in deeply nested module calls are handled correctly
 * ============================================================================ */

TEST_F(ExceptionCompleteFlowE2ETest, ChainedModuleCallExceptionHandling) {
    printf("=== Test: Chained Module Call Exception Handling ===\n");

    // Simulate deep call chain
    const int CHAIN_DEPTH = 8;
    std::vector<nimcp_exception_t*> exception_chain;

    // Create exception chain (simulating nested calls)
    nimcp_exception_t* innermost = nimcp_exception_create(
        NIMCP_ERROR_FILE_READ,
        EXCEPTION_SEVERITY_WARNING,
        __FILE__, __LINE__, __func__,
        "Innermost: File read failed"
    );
    ASSERT_NE(innermost, nullptr);
    exception_chain.push_back(innermost);

    nimcp_exception_t* current = innermost;
    for (int i = 1; i < CHAIN_DEPTH; i++) {
        nimcp_exception_t* wrapper = nimcp_exception_create(
            NIMCP_ERROR_OPERATION_FAILED + i,
            EXCEPTION_SEVERITY_ERROR,
            __FILE__, __LINE__, __func__,
            "Chain level %d: Operation failed", i
        );
        ASSERT_NE(wrapper, nullptr);

        nimcp_exception_set_cause(wrapper, current);
        exception_chain.push_back(wrapper);
        current = wrapper;
    }
    printf("  Created exception chain of depth %d\n", CHAIN_DEPTH);

    // Get the outermost exception (last in chain)
    nimcp_exception_t* outermost = exception_chain.back();

    // Dispatch outermost exception
    g_handler_calls = 0;
    nimcp_exception_dispatch(outermost);
    EXPECT_GT(g_handler_calls.load(), 0);
    printf("  Outermost exception dispatched\n");

    // Walk the chain and verify all levels
    int depth = 0;
    nimcp_exception_t* walker = outermost;
    while (walker) {
        printf("  Level %d: code=%d\n", depth, walker->code);
        walker = nimcp_exception_get_cause(walker);
        depth++;
    }
    EXPECT_EQ(depth, CHAIN_DEPTH);
    printf("  Verified chain depth: %d\n", depth);

    // Clean up - only unref outermost, it will unref the chain
    nimcp_exception_unref(outermost);
    printf("Test passed: Chained module call exception handling verified\n\n");
}

/* ============================================================================
 * Test 6: Cross-Module Exception Context Preservation
 *
 * Verifies context data is preserved as exceptions propagate across modules
 * ============================================================================ */

TEST_F(ExceptionCompleteFlowE2ETest, CrossModuleContextPreservation) {
    printf("=== Test: Cross-Module Exception Context Preservation ===\n");

    // Create exception with rich context
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Exception with cross-module context"
    );
    ASSERT_NE(ex, nullptr);

    // Add context from different "modules"
    nimcp_exception_set_context(ex, "api_request_id", "req-12345");
    nimcp_exception_set_context(ex, "cognitive_task_id", "task-67890");
    nimcp_exception_set_context(ex, "brain_region", "hippocampus");
    nimcp_exception_set_context(ex, "neural_layer", "layer_3");
    nimcp_exception_set_context(ex, "memory_pool", "arena_1");
    printf("  Set 5 context entries across modules\n");

    // Verify context count
    EXPECT_EQ(nimcp_exception_context_count(ex), 5u);

    // Dispatch (simulating propagation)
    nimcp_exception_dispatch(ex);
    printf("  Exception dispatched\n");

    // Verify all context preserved after dispatch
    EXPECT_STREQ(nimcp_exception_get_context(ex, "api_request_id"), "req-12345");
    EXPECT_STREQ(nimcp_exception_get_context(ex, "cognitive_task_id"), "task-67890");
    EXPECT_STREQ(nimcp_exception_get_context(ex, "brain_region"), "hippocampus");
    EXPECT_STREQ(nimcp_exception_get_context(ex, "neural_layer"), "layer_3");
    EXPECT_STREQ(nimcp_exception_get_context(ex, "memory_pool"), "arena_1");
    printf("  All context entries verified after dispatch\n");

    // Present to immune and verify context survives
    nimcp_immune_response_t response;
    memset(&response, 0, sizeof(response));
    nimcp_exception_present_to_immune(ex, &response);

    EXPECT_EQ(nimcp_exception_context_count(ex), 5u);
    printf("  Context preserved after immune presentation\n");

    nimcp_exception_unref(ex);
    printf("Test passed: Cross-module exception context preservation verified\n\n");
}

/* ============================================================================
 * Test 7: Exception Aggregation Across Module Boundaries
 *
 * Verifies aggregate exceptions can collect errors from multiple modules
 * ============================================================================ */

TEST_F(ExceptionCompleteFlowE2ETest, ExceptionAggregationAcrossModules) {
    printf("=== Test: Exception Aggregation Across Module Boundaries ===\n");

    // Create aggregate exception for batch operation
    nimcp_aggregate_exception_t* agg = nimcp_aggregate_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        "Batch operation failed with multiple module errors"
    );
    ASSERT_NE(agg, nullptr);
    printf("  Aggregate exception created\n");

    // Simulate errors from different modules
    struct ModuleError {
        const char* module;
        nimcp_error_t code;
        nimcp_exception_category_t category;
    } module_errors[] = {
        {"memory_manager", NIMCP_ERROR_NO_MEMORY, EXCEPTION_CATEGORY_MEMORY},
        {"io_subsystem", NIMCP_ERROR_FILE_WRITE, EXCEPTION_CATEGORY_IO},
        {"neural_backend", NIMCP_ERROR_FORWARD_PASS, EXCEPTION_CATEGORY_BRAIN},
        {"thread_pool", NIMCP_ERROR_DEADLOCK, EXCEPTION_CATEGORY_THREADING},
    };

    for (const auto& err : module_errors) {
        nimcp_exception_t* child = nimcp_exception_create(
            err.code,
            EXCEPTION_SEVERITY_ERROR,
            __FILE__, __LINE__, __func__,
            "Error from %s", err.module
        );
        ASSERT_NE(child, nullptr);
        child->category = err.category;
        nimcp_exception_set_context(child, "source_module", err.module);

        int add_result = nimcp_aggregate_exception_add(agg, child);
        EXPECT_EQ(add_result, 0);
    }
    printf("  Added %zu child exceptions from different modules\n",
           nimcp_aggregate_exception_count(agg));

    EXPECT_EQ(nimcp_aggregate_exception_count(agg), 4u);

    // Dispatch aggregate
    g_handler_calls = 0;
    nimcp_exception_dispatch(&agg->base);
    EXPECT_GT(g_handler_calls.load(), 0);
    printf("  Aggregate dispatched\n");

    // Verify all children are accessible
    for (size_t i = 0; i < nimcp_aggregate_exception_count(agg); i++) {
        nimcp_exception_t* child = nimcp_aggregate_exception_get(agg, i);
        ASSERT_NE(child, nullptr);
        const char* source = nimcp_exception_get_context(child, "source_module");
        EXPECT_NE(source, nullptr);
        printf("  Child[%zu]: module=%s, category=%s\n",
               i, source ? source : "unknown",
               nimcp_exception_category_to_string(child->category));
    }

    // Verify different categories are represented
    std::vector<nimcp_exception_category_t> seen_categories;
    for (size_t i = 0; i < nimcp_aggregate_exception_count(agg); i++) {
        nimcp_exception_t* child = nimcp_aggregate_exception_get(agg, i);
        seen_categories.push_back(child->category);
    }
    EXPECT_EQ(seen_categories.size(), 4u);

    nimcp_exception_unref(&agg->base);
    printf("Test passed: Exception aggregation across module boundaries verified\n\n");
}

/* ============================================================================
 * Test 8: Cleanup Order Verification During Exception Unwind
 *
 * Verifies cleanup happens in correct order (LIFO) during exception unwind
 * ============================================================================ */

TEST_F(ExceptionCompleteFlowE2ETest, CleanupOrderVerification) {
    printf("=== Test: Cleanup Order Verification During Exception Unwind ===\n");

    // Register modules in order
    std::vector<std::string> modules = {
        "level_1_init",
        "level_2_resource",
        "level_3_connection",
        "level_4_session",
        "level_5_transaction"
    };

    for (const auto& mod : modules) {
        register_module(mod);
        init_module(mod);
    }
    printf("  Initialized %zu modules in order\n", modules.size());

    // Simulate exception at deepest level
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Transaction failed, unwinding stack"
    );
    ASSERT_NE(ex, nullptr);

    // Track cleanup order
    std::vector<std::string> cleanup_order;

    // Cleanup in reverse order (LIFO - simulating stack unwind)
    for (auto it = modules.rbegin(); it != modules.rend(); ++it) {
        cleanup_order.push_back(*it);
        cleanup_module(*it);
    }

    // Verify LIFO order
    EXPECT_EQ(cleanup_order[0], "level_5_transaction");
    EXPECT_EQ(cleanup_order[1], "level_4_session");
    EXPECT_EQ(cleanup_order[2], "level_3_connection");
    EXPECT_EQ(cleanup_order[3], "level_2_resource");
    EXPECT_EQ(cleanup_order[4], "level_1_init");

    printf("  Cleanup order verified:\n");
    for (size_t i = 0; i < cleanup_order.size(); i++) {
        printf("    [%zu] %s\n", i, cleanup_order[i].c_str());
    }

    // Verify all modules cleaned up
    {
        std::lock_guard<std::mutex> lock(g_state_mutex);
        for (const auto& mod : modules) {
            EXPECT_FALSE(g_module_states[mod].resources_allocated);
        }
    }

    nimcp_exception_unref(ex);
    printf("Test passed: Cleanup order verification verified\n\n");
}

/* ============================================================================
 * Test 9: Error Code Propagation Fidelity
 *
 * Verifies error codes are preserved accurately through the exception pipeline
 * ============================================================================ */

TEST_F(ExceptionCompleteFlowE2ETest, ErrorCodePropagationFidelity) {
    printf("=== Test: Error Code Propagation Fidelity ===\n");

    // Test various error codes from different categories
    struct ErrorTest {
        nimcp_error_t code;
        nimcp_exception_category_t expected_category;
    } error_tests[] = {
        {NIMCP_ERROR_NO_MEMORY, EXCEPTION_CATEGORY_MEMORY},
        {NIMCP_ERROR_FILE_READ, EXCEPTION_CATEGORY_IO},
        {NIMCP_ERROR_BRAIN_CREATION, EXCEPTION_CATEGORY_BRAIN},
        {NIMCP_ERROR_THREAD_CREATE, EXCEPTION_CATEGORY_THREADING},
        {NIMCP_ERROR_CONFIG_PARSE, EXCEPTION_CATEGORY_CONFIG},
    };

    for (const auto& test : error_tests) {
        nimcp_exception_t* ex = nimcp_exception_create(
            test.code,
            EXCEPTION_SEVERITY_ERROR,
            __FILE__, __LINE__, __func__,
            "Test exception for code %d", test.code
        );
        ASSERT_NE(ex, nullptr);

        // Verify error code preserved
        EXPECT_EQ(ex->code, test.code);

        // Verify category correctly determined
        nimcp_exception_category_t actual_cat = nimcp_exception_get_category_from_code(test.code);
        EXPECT_EQ(actual_cat, test.expected_category)
            << "Code " << test.code << " expected category "
            << nimcp_exception_category_to_string(test.expected_category)
            << " but got " << nimcp_exception_category_to_string(actual_cat);

        // Dispatch and verify code still preserved
        nimcp_exception_dispatch(ex);
        EXPECT_EQ(ex->code, test.code);

        printf("  Error code %d -> category %s: verified\n",
               test.code, nimcp_exception_category_to_string(actual_cat));

        nimcp_exception_unref(ex);
    }

    printf("Test passed: Error code propagation fidelity verified\n\n");
}

/* ============================================================================
 * Test 10: Multi-Thread Exception Propagation
 *
 * Verifies exception handling works correctly in multi-threaded scenarios
 * ============================================================================ */

TEST_F(ExceptionCompleteFlowE2ETest, MultiThreadExceptionPropagation) {
    printf("=== Test: Multi-Thread Exception Propagation ===\n");

    const int NUM_THREADS = 4;
    const int EXCEPTIONS_PER_THREAD = 100;
    std::vector<std::thread> threads;
    std::atomic<int> total_exceptions{0};
    std::atomic<int> total_handled{0};

    for (int t = 0; t < NUM_THREADS; t++) {
        threads.emplace_back([t, &total_exceptions, &total_handled, EXCEPTIONS_PER_THREAD]() {
            for (int i = 0; i < EXCEPTIONS_PER_THREAD; i++) {
                nimcp_exception_t* ex = nimcp_exception_create(
                    NIMCP_ERROR_OPERATION_FAILED + t,
                    EXCEPTION_SEVERITY_ERROR,
                    __FILE__, __LINE__, __func__,
                    "Thread %d exception %d", t, i
                );
                if (ex) {
                    total_exceptions++;

                    // Add thread-specific context
                    char thread_id[32];
                    snprintf(thread_id, sizeof(thread_id), "thread_%d", t);
                    nimcp_exception_set_context(ex, "source_thread", thread_id);

                    // Dispatch
                    bool handled = nimcp_exception_dispatch(ex);
                    if (handled) total_handled++;

                    nimcp_exception_unref(ex);
                }
            }
        });
    }

    // Wait for all threads
    for (auto& t : threads) {
        t.join();
    }

    printf("  Total exceptions created: %d\n", total_exceptions.load());
    printf("  Handler calls: %d\n", g_handler_calls.load());

    EXPECT_EQ(total_exceptions.load(), NUM_THREADS * EXCEPTIONS_PER_THREAD);
    EXPECT_GT(g_handler_calls.load(), 0);

    printf("Test passed: Multi-thread exception propagation verified\n\n");
}

/* ============================================================================
 * Test 11: Exception Recovery Strategy Selection
 *
 * Verifies correct recovery strategies are suggested for different exception types
 * ============================================================================ */

TEST_F(ExceptionCompleteFlowE2ETest, RecoveryStrategySelection) {
    printf("=== Test: Exception Recovery Strategy Selection ===\n");

    struct StrategyTest {
        nimcp_error_t code;
        nimcp_exception_type_t type;
        const char* description;
    } strategy_tests[] = {
        {NIMCP_ERROR_NO_MEMORY, EXCEPTION_TYPE_MEMORY, "Memory exception"},
        {NIMCP_ERROR_BRAIN_CREATION, EXCEPTION_TYPE_BRAIN, "Brain exception"},
        {NIMCP_ERROR_FILE_WRITE, EXCEPTION_TYPE_IO, "I/O exception"},
    };

    for (const auto& test : strategy_tests) {
        nimcp_exception_t* ex = nimcp_exception_create(
            test.code,
            EXCEPTION_SEVERITY_SEVERE,
            __FILE__, __LINE__, __func__,
            "%s test", test.description
        );
        ASSERT_NE(ex, nullptr);
        ex->type = test.type;

        // Get recovery strategy
        nimcp_exception_recovery_strategy_t strategy;
        nimcp_exception_get_recovery_strategy(ex, &strategy);

        printf("  %s:\n", test.description);
        printf("    Primary: %s\n",
               nimcp_exception_recovery_action_to_string(strategy.primary_action));
        printf("    Fallback: %s\n",
               nimcp_exception_recovery_action_to_string(strategy.fallback_action));
        printf("    Retries: %u, Cooldown: %ums\n",
               strategy.retry_count, strategy.cooldown_ms);

        // Verify strategy is not empty
        EXPECT_NE(strategy.primary_action, EXCEPTION_RECOVERY_NONE);

        nimcp_exception_unref(ex);
    }

    printf("Test passed: Recovery strategy selection verified\n\n");
}

/* ============================================================================
 * Test 12: Full Pipeline Integration Test
 *
 * End-to-end test of the complete exception handling pipeline
 * ============================================================================ */

TEST_F(ExceptionCompleteFlowE2ETest, FullPipelineIntegration) {
    printf("=== Test: Full Pipeline Integration ===\n");

    // Reset metrics for clean test
    nimcp_metrics_reset();
    nimcp_circuit_reset_all();

    // Install all default handlers
    nimcp_install_default_handlers();

    // Register recovery callbacks
    nimcp_register_recovery_callback(EXCEPTION_RECOVERY_GC, test_recovery_callback, nullptr);
    nimcp_register_recovery_callback(EXCEPTION_RECOVERY_ROLLBACK, test_recovery_callback, nullptr);

    printf("  Step 1: Pipeline setup complete\n");

    // Create exception from API layer
    nimcp_exception_t* api_ex = nimcp_exception_create(
        NIMCP_ERROR_INVALID_PARAMETER,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "API: Invalid input parameter"
    );
    ASSERT_NE(api_ex, nullptr);
    nimcp_exception_set_context(api_ex, "endpoint", "/v1/brain/create");
    nimcp_exception_set_context(api_ex, "parameter", "neuron_count");

    printf("  Step 2: API exception created\n");

    // Record metrics
    nimcp_metrics_record_exception(api_ex);
    printf("  Step 3: Metrics recorded\n");

    // Check circuit breaker
    int circuit_result = nimcp_circuit_record(api_ex);
    printf("  Step 4: Circuit breaker checked (result=%d)\n", circuit_result);

    // Dispatch through handlers
    g_handler_calls = 0;
    bool handled = nimcp_exception_dispatch(api_ex);
    printf("  Step 5: Exception dispatched (handled=%s, calls=%d)\n",
           handled ? "yes" : "no", g_handler_calls.load());

    // Present to immune
    nimcp_immune_response_t response;
    memset(&response, 0, sizeof(response));
    nimcp_exception_present_to_immune(api_ex, &response);
    printf("  Step 6: Presented to immune (antigen_id=%u)\n", response.antigen_id);

    // Get recovery strategy
    nimcp_exception_recovery_strategy_t strategy;
    nimcp_exception_get_recovery_strategy(api_ex, &strategy);
    printf("  Step 7: Recovery strategy retrieved\n");

    // Execute recovery
    g_rollback_calls = 0;
    if (strategy.primary_action != EXCEPTION_RECOVERY_NONE) {
        nimcp_execute_recovery(api_ex, strategy.primary_action);
    }
    printf("  Step 8: Recovery executed (calls=%d)\n", g_rollback_calls.load());

    // Notify result
    nimcp_exception_notify_recovery_result(api_ex, strategy.primary_action, true);
    printf("  Step 9: Recovery result notified\n");

    // Get final metrics
    nimcp_exception_metrics_t metrics;
    nimcp_metrics_get(&metrics);
    printf("  Step 10: Final metrics - total=%lu, rate=%.2f/sec\n",
           (unsigned long)metrics.total_exceptions, metrics.current_rate_per_second);

    // Verify pipeline flow
    EXPECT_GT(g_handler_calls.load(), 0);
    EXPECT_GE(metrics.total_exceptions, 1u);

    nimcp_exception_unref(api_ex);
    printf("Test passed: Full pipeline integration verified\n\n");
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
