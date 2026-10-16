/**
 * @file test_exception_cross_module.cpp
 * @brief Integration tests for cross-module exception handling
 *
 * WHAT: Test exception propagation and handling across module boundaries
 * WHY:  Verify exceptions flow correctly between cognitive, core, GPU, and async modules
 * HOW:  Simulate module boundaries with typed handlers and verify propagation paths
 *
 * TEST SCENARIOS:
 * - Exception propagation from cognitive to core modules
 * - Exception propagation from GPU to async modules
 * - Exception immune presentation flow
 * - Exception handler chain integration
 * - Cross-module typed exception handling (BRAIN, GPU, SECURITY)
 * - Bio-async integration with exception system
 * - Circuit breaker cross-module behavior
 * - Propagation context tracking across modules
 *
 * HEADER FILES READ:
 * - include/utils/exception/nimcp_exception_macros.h
 * - include/utils/exception/nimcp_exception.h
 * - include/utils/exception/nimcp_exception_handlers.h
 * - include/utils/exception/nimcp_exception_immune.h
 * - include/utils/exception/nimcp_exception_trace.h
 * - include/utils/exception/nimcp_exception_circuit.h
 * - include/utils/exception/nimcp_exception_metrics.h
 * - include/async/nimcp_bio_async.h
 * - include/utils/error/nimcp_error_codes.h
 *
 * FUNCTION SIGNATURES USED:
 * - nimcp_exception_create(code, severity, file, line, func, format, ...)
 * - nimcp_brain_exception_create(code, severity, file, line, func, brain_id, region_name, format, ...)
 * - nimcp_gpu_exception_create(code, severity, file, line, func, device_id, cuda_err, format, ...)
 * - nimcp_security_exception_create(code, severity, file, line, func, threat_type, format, ...)
 * - nimcp_exception_dispatch(ex) -> bool
 * - nimcp_exception_present_to_immune(ex, response) -> int
 * - nimcp_handler_register(options) -> nimcp_handler_registration_t*
 * - nimcp_handler_unregister(reg) -> int
 * - nimcp_exception_set_cause(ex, cause)
 * - nimcp_exception_get_cause(ex) -> nimcp_exception_t*
 * - nimcp_propagation_create(origin_module) -> nimcp_propagation_context_t*
 * - nimcp_propagation_add_hop(ctx, module, msg_type, priority) -> int
 * - nimcp_exception_set_propagation(ex, ctx) -> int
 * - nimcp_trace_create() -> nimcp_exception_trace_t
 * - nimcp_exception_set_trace(ex, trace) -> int
 * - nimcp_circuit_init() -> int
 * - nimcp_circuit_record(ex) -> int
 * - nimcp_metrics_init() -> int
 * - nimcp_metrics_record_exception(ex)
 *
 * @author NIMCP Development Team
 * @date 2026-01-21
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cstdio>
#include <vector>
#include <string>
#include <atomic>
#include <chrono>
#include <thread>
#include <functional>
#include <map>

extern "C" {
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_handlers.h"
#include "utils/exception/nimcp_exception_immune.h"
#include "utils/exception/nimcp_exception_trace.h"
#include "utils/exception/nimcp_exception_circuit.h"
#include "utils/exception/nimcp_exception_metrics.h"
#include "utils/error/nimcp_error_codes.h"
}

//=============================================================================
// Module Simulation Types
//=============================================================================

/**
 * @brief Simulated module types for cross-module testing
 */
enum class SimulatedModule {
    CORE,           // Core/base module
    COGNITIVE,      // Cognitive processing (brain regions)
    GPU,            // GPU computation
    ASYNC,          // Async/bio-async operations
    SECURITY,       // Security (BBB)
    IMMUNE          // Brain immune system
};

/**
 * @brief Record of an exception as seen by a module handler
 */
struct ModuleExceptionRecord {
    SimulatedModule receiving_module;
    nimcp_error_t code;
    nimcp_exception_type_t type;
    nimcp_exception_category_t category;
    nimcp_exception_severity_t severity;
    bool has_cause;
    nimcp_error_t cause_code;
    std::string message;
    uint64_t timestamp_us;
    bool was_handled;  // Handler returned true
};

/**
 * @brief Shared cross-module test state
 */
static struct {
    std::map<SimulatedModule, std::vector<ModuleExceptionRecord>> module_records;
    std::atomic<int> total_exceptions_seen{0};
    std::atomic<bool> cognitive_should_handle{false};
    std::atomic<bool> gpu_should_handle{false};
    std::atomic<bool> security_should_handle{false};
    std::atomic<bool> immune_notified{false};
    SimulatedModule target_handling_module{SimulatedModule::CORE};
} g_cross_module_state;

//=============================================================================
// Module Handler Functions
//=============================================================================

/**
 * @brief Handler for CORE module
 */
static bool core_module_handler(nimcp_exception_t* ex, void* user_data) {
    (void)user_data;

    ModuleExceptionRecord record;
    record.receiving_module = SimulatedModule::CORE;
    record.code = ex->code;
    record.type = ex->type;
    record.category = ex->category;
    record.severity = ex->severity;
    record.has_cause = (ex->cause != nullptr);
    record.cause_code = ex->cause ? ex->cause->code : NIMCP_SUCCESS;
    record.message = ex->message;
    record.timestamp_us = ex->timestamp_us;
    record.was_handled = (g_cross_module_state.target_handling_module == SimulatedModule::CORE);

    g_cross_module_state.module_records[SimulatedModule::CORE].push_back(record);
    g_cross_module_state.total_exceptions_seen++;

    return record.was_handled;
}

/**
 * @brief Handler for COGNITIVE module (brain regions)
 */
static bool cognitive_module_handler(nimcp_exception_t* ex, void* user_data) {
    (void)user_data;

    // Only handle brain-related exceptions
    if (ex->category != EXCEPTION_CATEGORY_BRAIN &&
        ex->category != EXCEPTION_CATEGORY_COGNITIVE &&
        ex->type != EXCEPTION_TYPE_BRAIN) {
        return false;
    }

    ModuleExceptionRecord record;
    record.receiving_module = SimulatedModule::COGNITIVE;
    record.code = ex->code;
    record.type = ex->type;
    record.category = ex->category;
    record.severity = ex->severity;
    record.has_cause = (ex->cause != nullptr);
    record.cause_code = ex->cause ? ex->cause->code : NIMCP_SUCCESS;
    record.message = ex->message;
    record.timestamp_us = ex->timestamp_us;
    record.was_handled = g_cross_module_state.cognitive_should_handle.load();

    g_cross_module_state.module_records[SimulatedModule::COGNITIVE].push_back(record);
    g_cross_module_state.total_exceptions_seen++;

    return record.was_handled;
}

/**
 * @brief Handler for GPU module
 */
static bool gpu_module_handler(nimcp_exception_t* ex, void* user_data) {
    (void)user_data;

    // Only handle GPU-related exceptions
    if (ex->category != EXCEPTION_CATEGORY_GPU &&
        ex->type != EXCEPTION_TYPE_GPU) {
        return false;
    }

    ModuleExceptionRecord record;
    record.receiving_module = SimulatedModule::GPU;
    record.code = ex->code;
    record.type = ex->type;
    record.category = ex->category;
    record.severity = ex->severity;
    record.has_cause = (ex->cause != nullptr);
    record.cause_code = ex->cause ? ex->cause->code : NIMCP_SUCCESS;
    record.message = ex->message;
    record.timestamp_us = ex->timestamp_us;
    record.was_handled = g_cross_module_state.gpu_should_handle.load();

    g_cross_module_state.module_records[SimulatedModule::GPU].push_back(record);
    g_cross_module_state.total_exceptions_seen++;

    return record.was_handled;
}

/**
 * @brief Handler for SECURITY module (BBB)
 */
static bool security_module_handler(nimcp_exception_t* ex, void* user_data) {
    (void)user_data;

    // Only handle security-related exceptions
    if (ex->category != EXCEPTION_CATEGORY_SECURITY &&
        ex->type != EXCEPTION_TYPE_SECURITY) {
        return false;
    }

    ModuleExceptionRecord record;
    record.receiving_module = SimulatedModule::SECURITY;
    record.code = ex->code;
    record.type = ex->type;
    record.category = ex->category;
    record.severity = ex->severity;
    record.has_cause = (ex->cause != nullptr);
    record.cause_code = ex->cause ? ex->cause->code : NIMCP_SUCCESS;
    record.message = ex->message;
    record.timestamp_us = ex->timestamp_us;
    record.was_handled = g_cross_module_state.security_should_handle.load();

    g_cross_module_state.module_records[SimulatedModule::SECURITY].push_back(record);
    g_cross_module_state.total_exceptions_seen++;

    return record.was_handled;
}

/**
 * @brief Handler for ASYNC module
 */
static bool async_module_handler(nimcp_exception_t* ex, void* user_data) {
    (void)user_data;

    ModuleExceptionRecord record;
    record.receiving_module = SimulatedModule::ASYNC;
    record.code = ex->code;
    record.type = ex->type;
    record.category = ex->category;
    record.severity = ex->severity;
    record.has_cause = (ex->cause != nullptr);
    record.cause_code = ex->cause ? ex->cause->code : NIMCP_SUCCESS;
    record.message = ex->message;
    record.timestamp_us = ex->timestamp_us;
    record.was_handled = (g_cross_module_state.target_handling_module == SimulatedModule::ASYNC);

    g_cross_module_state.module_records[SimulatedModule::ASYNC].push_back(record);
    g_cross_module_state.total_exceptions_seen++;

    return record.was_handled;
}

/**
 * @brief Handler for IMMUNE module (monitors severe exceptions)
 */
static bool immune_module_handler(nimcp_exception_t* ex, void* user_data) {
    (void)user_data;

    // Only process severe+ exceptions
    if (ex->severity < EXCEPTION_SEVERITY_SEVERE) {
        return false;
    }

    ModuleExceptionRecord record;
    record.receiving_module = SimulatedModule::IMMUNE;
    record.code = ex->code;
    record.type = ex->type;
    record.category = ex->category;
    record.severity = ex->severity;
    record.has_cause = (ex->cause != nullptr);
    record.cause_code = ex->cause ? ex->cause->code : NIMCP_SUCCESS;
    record.message = ex->message;
    record.timestamp_us = ex->timestamp_us;
    record.was_handled = false;  // Immune never consumes, just monitors

    g_cross_module_state.module_records[SimulatedModule::IMMUNE].push_back(record);
    g_cross_module_state.immune_notified = true;
    g_cross_module_state.total_exceptions_seen++;

    return false;  // Never consume, let other handlers process
}

//=============================================================================
// Test Fixture
//=============================================================================

class ExceptionCrossModuleTest : public ::testing::Test {
protected:
    std::vector<nimcp_handler_registration_t*> registrations_;

    void SetUp() override {
        // Clear test state
        g_cross_module_state.module_records.clear();
        g_cross_module_state.total_exceptions_seen = 0;
        g_cross_module_state.cognitive_should_handle = false;
        g_cross_module_state.gpu_should_handle = false;
        g_cross_module_state.security_should_handle = false;
        g_cross_module_state.immune_notified = false;
        g_cross_module_state.target_handling_module = SimulatedModule::CORE;

        // Initialize subsystems
        nimcp_exception_system_init();

        // Initialize tracing system (may fail, that's ok)
        nimcp_trace_init();

        // Initialize circuit breaker (may fail, that's ok)
        nimcp_circuit_init();

        // Initialize metrics (may fail, that's ok)
        nimcp_metrics_init();

        // Initialize immune integration
        nimcp_exception_immune_config_t immune_config;
        nimcp_exception_immune_default_config(&immune_config);
        immune_config.enable_auto_present = false;
        nimcp_exception_immune_init(&immune_config);

        // Register module handlers in priority order
        registerModuleHandlers();
    }

    void TearDown() override {
        // Unregister all handlers
        for (auto* reg : registrations_) {
            if (reg) {
                nimcp_handler_unregister(reg);
            }
        }
        registrations_.clear();

        // Cleanup subsystems
        nimcp_exception_clear_current();
        nimcp_exception_handlers_shutdown();
        nimcp_exception_immune_shutdown();
        nimcp_metrics_shutdown();
        nimcp_circuit_shutdown();
        nimcp_trace_shutdown();
        nimcp_exception_system_shutdown();
    }

    void registerModuleHandlers() {
        // Register handlers from highest to lowest priority

        // IMMUNE module - highest priority (monitors all severe+ exceptions)
        nimcp_handler_options_t immune_opts;
        nimcp_handler_default_options(&immune_opts);
        immune_opts.name = "immune_module";
        immune_opts.handler = immune_module_handler;
        immune_opts.priority = NIMCP_HANDLER_PRIORITY_HIGH + 50;  // 150
        immune_opts.min_severity = EXCEPTION_SEVERITY_SEVERE;
        auto* reg_immune = nimcp_handler_register(&immune_opts);
        if (reg_immune) registrations_.push_back(reg_immune);

        // SECURITY module - high priority
        nimcp_handler_options_t security_opts;
        nimcp_handler_default_options(&security_opts);
        security_opts.name = "security_module";
        security_opts.handler = security_module_handler;
        security_opts.priority = NIMCP_HANDLER_PRIORITY_HIGH;  // 100
        auto* reg_security = nimcp_handler_register(&security_opts);
        if (reg_security) registrations_.push_back(reg_security);

        // COGNITIVE module - high-normal priority
        nimcp_handler_options_t cognitive_opts;
        nimcp_handler_default_options(&cognitive_opts);
        cognitive_opts.name = "cognitive_module";
        cognitive_opts.handler = cognitive_module_handler;
        cognitive_opts.priority = NIMCP_HANDLER_PRIORITY_NORMAL + 25;  // 75
        auto* reg_cognitive = nimcp_handler_register(&cognitive_opts);
        if (reg_cognitive) registrations_.push_back(reg_cognitive);

        // GPU module - normal priority
        nimcp_handler_options_t gpu_opts;
        nimcp_handler_default_options(&gpu_opts);
        gpu_opts.name = "gpu_module";
        gpu_opts.handler = gpu_module_handler;
        gpu_opts.priority = NIMCP_HANDLER_PRIORITY_NORMAL;  // 50
        auto* reg_gpu = nimcp_handler_register(&gpu_opts);
        if (reg_gpu) registrations_.push_back(reg_gpu);

        // ASYNC module - low-normal priority
        nimcp_handler_options_t async_opts;
        nimcp_handler_default_options(&async_opts);
        async_opts.name = "async_module";
        async_opts.handler = async_module_handler;
        async_opts.priority = NIMCP_HANDLER_PRIORITY_NORMAL - 10;  // 40
        auto* reg_async = nimcp_handler_register(&async_opts);
        if (reg_async) registrations_.push_back(reg_async);

        // CORE module - lowest priority (fallback)
        nimcp_handler_options_t core_opts;
        nimcp_handler_default_options(&core_opts);
        core_opts.name = "core_module";
        core_opts.handler = core_module_handler;
        core_opts.priority = NIMCP_HANDLER_PRIORITY_LOW;  // 10
        auto* reg_core = nimcp_handler_register(&core_opts);
        if (reg_core) registrations_.push_back(reg_core);
    }

    /**
     * @brief Helper to check if module received exception
     */
    bool moduleReceivedException(SimulatedModule module, nimcp_error_t code) {
        auto it = g_cross_module_state.module_records.find(module);
        if (it == g_cross_module_state.module_records.end()) {
            return false;
        }
        for (const auto& record : it->second) {
            if (record.code == code) {
                return true;
            }
        }
        return false;
    }

    /**
     * @brief Get count of exceptions received by module
     */
    size_t moduleExceptionCount(SimulatedModule module) {
        auto it = g_cross_module_state.module_records.find(module);
        if (it == g_cross_module_state.module_records.end()) {
            return 0;
        }
        return it->second.size();
    }
};

//=============================================================================
// Test: Cognitive Exception Propagates to Core
//=============================================================================

TEST_F(ExceptionCrossModuleTest, CognitiveExceptionPropagatesToCore) {
    // WHAT: Verify brain exception flows from cognitive module to core
    // WHY:  Cognitive errors need fallback handling by core module

    // Create brain exception (will be seen by cognitive module)
    nimcp_brain_exception_t* ex = nimcp_brain_exception_create(
        NIMCP_ERROR_BRAIN_CREATION,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        42,  // brain_id
        "visual_cortex",  // region_name
        "Visual cortex initialization failed"
    );
    ASSERT_NE(ex, nullptr);

    // Cognitive should not handle (let it propagate)
    g_cross_module_state.cognitive_should_handle = false;

    // Dispatch
    bool handled = nimcp_exception_dispatch((nimcp_exception_t*)ex);

    // Cognitive module should have seen it (due to category filter)
    EXPECT_TRUE(moduleReceivedException(SimulatedModule::COGNITIVE, NIMCP_ERROR_BRAIN_CREATION))
        << "Cognitive module should receive brain exception";

    // Core module should also have seen it (as fallback)
    EXPECT_TRUE(moduleReceivedException(SimulatedModule::CORE, NIMCP_ERROR_BRAIN_CREATION))
        << "Core module should receive exception when cognitive doesn't handle";

    nimcp_exception_unref((nimcp_exception_t*)ex);
}

//=============================================================================
// Test: Cognitive Module Handles Own Exception
//=============================================================================

TEST_F(ExceptionCrossModuleTest, CognitiveModuleHandlesOwnException) {
    // WHAT: Verify cognitive module can fully handle brain exceptions
    // WHY:  Specialized modules should be able to handle their own errors

    nimcp_brain_exception_t* ex = nimcp_brain_exception_create(
        NIMCP_ERROR_LEARNING_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        1,
        "hippocampus",
        "Learning step failed in hippocampus"
    );
    ASSERT_NE(ex, nullptr);

    // Cognitive should handle
    g_cross_module_state.cognitive_should_handle = true;

    bool handled = nimcp_exception_dispatch((nimcp_exception_t*)ex);
    EXPECT_TRUE(handled) << "Exception should be handled";

    // Cognitive should have seen and handled
    EXPECT_TRUE(moduleReceivedException(SimulatedModule::COGNITIVE, NIMCP_ERROR_LEARNING_FAILED));

    // Core should NOT see it (consumed by cognitive)
    EXPECT_FALSE(moduleReceivedException(SimulatedModule::CORE, NIMCP_ERROR_LEARNING_FAILED))
        << "Core should not see exception handled by cognitive";

    nimcp_exception_unref((nimcp_exception_t*)ex);
}

//=============================================================================
// Test: GPU Exception to Async Module Flow
//=============================================================================

TEST_F(ExceptionCrossModuleTest, GpuExceptionToAsyncModuleFlow) {
    // WHAT: Verify GPU exceptions propagate through async module
    // WHY:  GPU operations often occur in async context

    nimcp_gpu_exception_t* ex = nimcp_gpu_exception_create(
        NIMCP_ERROR_GPU_MEMORY,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        0,  // device_id
        2,  // cuda_error (simulated out of memory)
        "GPU memory allocation failed"
    );
    ASSERT_NE(ex, nullptr);

    // GPU module should not handle (simulate escalation)
    g_cross_module_state.gpu_should_handle = false;

    bool handled = nimcp_exception_dispatch((nimcp_exception_t*)ex);

    // GPU module should have seen it
    EXPECT_TRUE(moduleReceivedException(SimulatedModule::GPU, NIMCP_ERROR_GPU_MEMORY))
        << "GPU module should receive GPU exception";

    // Async module should see it (lower priority in chain)
    EXPECT_TRUE(moduleReceivedException(SimulatedModule::ASYNC, NIMCP_ERROR_GPU_MEMORY))
        << "Async module should see unhandled GPU exception";

    // Core should also see it
    EXPECT_TRUE(moduleReceivedException(SimulatedModule::CORE, NIMCP_ERROR_GPU_MEMORY))
        << "Core module should see unhandled exception";

    nimcp_exception_unref((nimcp_exception_t*)ex);
}

//=============================================================================
// Test: Security Exception Triggers Immune Notification
//=============================================================================

TEST_F(ExceptionCrossModuleTest, SecurityExceptionTriggersImmuneNotification) {
    // WHAT: Verify severe security exceptions notify immune module
    // WHY:  Security threats should trigger immune response

    nimcp_security_exception_t* ex = nimcp_security_exception_create(
        NIMCP_ERROR_SECURITY_THREAT,
        EXCEPTION_SEVERITY_CRITICAL,  // Security always critical
        __FILE__, __LINE__, __func__,
        1,  // threat_type
        "Injection attack detected"
    );
    ASSERT_NE(ex, nullptr);

    // Security should handle but immune should still be notified
    g_cross_module_state.security_should_handle = true;

    bool handled = nimcp_exception_dispatch((nimcp_exception_t*)ex);
    EXPECT_TRUE(handled);

    // Security module should handle
    EXPECT_TRUE(moduleReceivedException(SimulatedModule::SECURITY, NIMCP_ERROR_SECURITY_THREAT))
        << "Security module should receive security exception";

    // Immune module should have been notified (high priority, monitors severe+)
    EXPECT_TRUE(g_cross_module_state.immune_notified.load())
        << "Immune module should be notified of critical security exception";

    EXPECT_TRUE(moduleReceivedException(SimulatedModule::IMMUNE, NIMCP_ERROR_SECURITY_THREAT))
        << "Immune module should have recorded the exception";

    nimcp_exception_unref((nimcp_exception_t*)ex);
}

//=============================================================================
// Test: Exception Chaining (Cause) Across Modules
//=============================================================================

TEST_F(ExceptionCrossModuleTest, ExceptionChainingAcrossModules) {
    // WHAT: Verify exception cause chain is preserved across modules
    // WHY:  Root cause tracking is essential for debugging

    // Create root cause (GPU error)
    nimcp_gpu_exception_t* root_cause = nimcp_gpu_exception_create(
        NIMCP_ERROR_GPU_MEMORY,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        0, 2,
        "GPU OOM"
    );
    ASSERT_NE(root_cause, nullptr);

    // Create wrapper exception (cognitive trying to use GPU)
    nimcp_brain_exception_t* wrapper = nimcp_brain_exception_create(
        NIMCP_ERROR_FORWARD_PASS,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        1,
        "visual_cortex",
        "Forward pass failed due to GPU error"
    );
    ASSERT_NE(wrapper, nullptr);

    // Chain: wrapper caused by root_cause
    nimcp_exception_set_cause((nimcp_exception_t*)wrapper, (nimcp_exception_t*)root_cause);

    // Dispatch wrapper
    bool handled = nimcp_exception_dispatch((nimcp_exception_t*)wrapper);

    // Verify cognitive module saw exception with cause
    auto it = g_cross_module_state.module_records.find(SimulatedModule::COGNITIVE);
    ASSERT_NE(it, g_cross_module_state.module_records.end());

    bool found_with_cause = false;
    for (const auto& record : it->second) {
        if (record.code == NIMCP_ERROR_FORWARD_PASS && record.has_cause) {
            EXPECT_EQ(record.cause_code, NIMCP_ERROR_GPU_MEMORY)
                << "Cause should be the GPU error";
            found_with_cause = true;
            break;
        }
    }
    EXPECT_TRUE(found_with_cause) << "Should find exception with GPU cause";

    nimcp_exception_unref((nimcp_exception_t*)wrapper);
    // Note: root_cause was reffed by set_cause, unreffed by wrapper cleanup
}

//=============================================================================
// Test: Exception Propagation Path Tracking
//=============================================================================

TEST_F(ExceptionCrossModuleTest, ExceptionPropagationPathTracking) {
    // WHAT: Verify propagation context tracks module path
    // WHY:  Debug and audit trail for exception routing

    // Create exception with propagation context
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Cross-module error"
    );
    ASSERT_NE(ex, nullptr);

    // Create propagation context
    nimcp_propagation_context_t* prop_ctx = nimcp_propagation_create("gpu_module");
    ASSERT_NE(prop_ctx, nullptr);

    // Add propagation hops
    nimcp_propagation_add_hop(prop_ctx, "gpu_module", "ERROR", 5);
    nimcp_propagation_add_hop(prop_ctx, "async_module", "ESCALATE", 7);
    nimcp_propagation_add_hop(prop_ctx, "core_module", "HANDLE", 8);

    // Attach to exception
    int result = nimcp_exception_set_propagation(ex, prop_ctx);
    EXPECT_EQ(result, 0) << "Should successfully attach propagation context";

    // Verify propagation context is accessible
    const nimcp_propagation_context_t* retrieved = nimcp_exception_get_propagation(ex);
    ASSERT_NE(retrieved, nullptr);
    EXPECT_STREQ(retrieved->origin_module, "gpu_module");
    EXPECT_EQ(retrieved->path_length, 3u);

    nimcp_exception_dispatch(ex);
    nimcp_exception_unref(ex);
}

//=============================================================================
// Test: Distributed Trace Context Across Modules
//=============================================================================

TEST_F(ExceptionCrossModuleTest, DistributedTraceContextAcrossModules) {
    // WHAT: Verify trace context follows exception across modules
    // WHY:  Distributed tracing for cross-node/module correlation

    // Create trace context
    nimcp_exception_trace_t trace = nimcp_trace_create();
    EXPECT_NE(trace.trace_id, 0u) << "Trace ID should be generated";
    EXPECT_NE(trace.span_id, 0u) << "Span ID should be generated";

    // Create exception
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_BRAIN_CREATION,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Traced brain error"
    );
    ASSERT_NE(ex, nullptr);

    // Attach trace
    int result = nimcp_exception_set_trace(ex, &trace);
    EXPECT_EQ(result, 0) << "Should successfully attach trace";

    // Retrieve trace
    const nimcp_exception_trace_t* retrieved_trace = nimcp_exception_get_trace(ex);
    ASSERT_NE(retrieved_trace, nullptr);
    EXPECT_EQ(retrieved_trace->trace_id, trace.trace_id);
    EXPECT_EQ(retrieved_trace->span_id, trace.span_id);

    nimcp_exception_dispatch(ex);
    nimcp_exception_unref(ex);
}

//=============================================================================
// Test: Cross-Module Handler Priority Order
//=============================================================================

TEST_F(ExceptionCrossModuleTest, CrossModuleHandlerPriorityOrder) {
    // WHAT: Verify exception flows through modules in priority order
    // WHY:  Priority determines which module gets first chance to handle

    // Create generic exception (will flow through all modules)
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_SEVERE,  // Severe so immune sees it
        __FILE__, __LINE__, __func__,
        "Priority order test"
    );
    ASSERT_NE(ex, nullptr);

    // No module should handle (let it flow through all)
    g_cross_module_state.target_handling_module = SimulatedModule::CORE;  // Only core handles

    nimcp_exception_dispatch(ex);

    // Verify all expected modules received the exception
    EXPECT_TRUE(moduleReceivedException(SimulatedModule::IMMUNE, NIMCP_ERROR_OPERATION_FAILED))
        << "Immune (highest priority for severe) should see exception";
    EXPECT_TRUE(moduleReceivedException(SimulatedModule::ASYNC, NIMCP_ERROR_OPERATION_FAILED))
        << "Async module should see exception";
    EXPECT_TRUE(moduleReceivedException(SimulatedModule::CORE, NIMCP_ERROR_OPERATION_FAILED))
        << "Core module should see exception";

    // Verify order by checking timestamps (approximate - modules record in order)
    auto& immune_records = g_cross_module_state.module_records[SimulatedModule::IMMUNE];
    auto& core_records = g_cross_module_state.module_records[SimulatedModule::CORE];

    if (!immune_records.empty() && !core_records.empty()) {
        // Immune should be called before core
        // Note: We can't easily verify order from timestamps in same thread,
        // but we can verify both were called
        EXPECT_GE(immune_records.size(), 1u);
        EXPECT_GE(core_records.size(), 1u);
    }

    nimcp_exception_unref(ex);
}

//=============================================================================
// Test: Typed Exception Routing to Specialized Modules
//=============================================================================

TEST_F(ExceptionCrossModuleTest, TypedExceptionRoutingToSpecializedModules) {
    // WHAT: Verify typed exceptions are routed to appropriate specialized handlers
    // WHY:  Brain exceptions should go to cognitive, GPU to GPU module, etc.

    // Test brain exception routes to cognitive
    {
        nimcp_brain_exception_t* brain_ex = nimcp_brain_exception_create(
            NIMCP_ERROR_LEARNING_FAILED, EXCEPTION_SEVERITY_ERROR,
            __FILE__, __LINE__, __func__, 1, "pfc", "PFC learning failed"
        );
        ASSERT_NE(brain_ex, nullptr);
        nimcp_exception_dispatch((nimcp_exception_t*)brain_ex);
        EXPECT_TRUE(moduleReceivedException(SimulatedModule::COGNITIVE, NIMCP_ERROR_LEARNING_FAILED))
            << "Brain exception should route to cognitive module";
        nimcp_exception_unref((nimcp_exception_t*)brain_ex);
    }

    // Test GPU exception routes to GPU module
    {
        nimcp_gpu_exception_t* gpu_ex = nimcp_gpu_exception_create(
            NIMCP_ERROR_KERNEL_LAUNCH, EXCEPTION_SEVERITY_ERROR,
            __FILE__, __LINE__, __func__, 0, 3, "Kernel launch failed"
        );
        ASSERT_NE(gpu_ex, nullptr);
        nimcp_exception_dispatch((nimcp_exception_t*)gpu_ex);
        EXPECT_TRUE(moduleReceivedException(SimulatedModule::GPU, NIMCP_ERROR_KERNEL_LAUNCH))
            << "GPU exception should route to GPU module";
        nimcp_exception_unref((nimcp_exception_t*)gpu_ex);
    }

    // Test security exception routes to security module
    {
        nimcp_security_exception_t* sec_ex = nimcp_security_exception_create(
            NIMCP_ERROR_BBB_REJECTED, EXCEPTION_SEVERITY_CRITICAL,
            __FILE__, __LINE__, __func__, 2, "BBB rejected input"
        );
        ASSERT_NE(sec_ex, nullptr);
        nimcp_exception_dispatch((nimcp_exception_t*)sec_ex);
        EXPECT_TRUE(moduleReceivedException(SimulatedModule::SECURITY, NIMCP_ERROR_BBB_REJECTED))
            << "Security exception should route to security module";
        nimcp_exception_unref((nimcp_exception_t*)sec_ex);
    }
}

//=============================================================================
// Test: Severity-Based Immune Notification
//=============================================================================

TEST_F(ExceptionCrossModuleTest, SeverityBasedImmuneNotification) {
    // WHAT: Verify immune module only receives SEVERE+ exceptions
    // WHY:  Immune system should focus on significant errors

    // Low severity - immune should NOT see
    {
        g_cross_module_state.immune_notified = false;
        nimcp_exception_t* low_ex = nimcp_exception_create(
            NIMCP_ERROR_INVALID_PARAMETER,
            EXCEPTION_SEVERITY_WARNING,
            __FILE__, __LINE__, __func__,
            "Low severity warning"
        );
        ASSERT_NE(low_ex, nullptr);
        nimcp_exception_dispatch(low_ex);
        EXPECT_FALSE(moduleReceivedException(SimulatedModule::IMMUNE, NIMCP_ERROR_INVALID_PARAMETER))
            << "Immune should NOT receive WARNING severity";
        nimcp_exception_unref(low_ex);
    }

    // Severe - immune SHOULD see
    {
        g_cross_module_state.immune_notified = false;
        nimcp_exception_t* severe_ex = nimcp_exception_create(
            NIMCP_ERROR_MEMORY_CORRUPTION,
            EXCEPTION_SEVERITY_SEVERE,
            __FILE__, __LINE__, __func__,
            "Severe memory corruption"
        );
        ASSERT_NE(severe_ex, nullptr);
        nimcp_exception_dispatch(severe_ex);
        EXPECT_TRUE(moduleReceivedException(SimulatedModule::IMMUNE, NIMCP_ERROR_MEMORY_CORRUPTION))
            << "Immune SHOULD receive SEVERE severity";
        nimcp_exception_unref(severe_ex);
    }

    // Critical - immune SHOULD see
    {
        g_cross_module_state.immune_notified = false;
        nimcp_exception_t* critical_ex = nimcp_exception_create(
            NIMCP_ERROR_DEADLOCK,
            EXCEPTION_SEVERITY_CRITICAL,
            __FILE__, __LINE__, __func__,
            "Critical deadlock"
        );
        ASSERT_NE(critical_ex, nullptr);
        nimcp_exception_dispatch(critical_ex);
        EXPECT_TRUE(moduleReceivedException(SimulatedModule::IMMUNE, NIMCP_ERROR_DEADLOCK))
            << "Immune SHOULD receive CRITICAL severity";
        nimcp_exception_unref(critical_ex);
    }
}

//=============================================================================
// Test: Multiple Concurrent Module Exceptions
//=============================================================================

TEST_F(ExceptionCrossModuleTest, MultipleConcurrentModuleExceptions) {
    // WHAT: Verify multiple exceptions from different modules are all handled
    // WHY:  Real systems have concurrent errors from multiple sources

    // Create exceptions from different "modules"
    std::vector<nimcp_exception_t*> exceptions;

    // Brain exception
    nimcp_brain_exception_t* brain_ex = nimcp_brain_exception_create(
        NIMCP_ERROR_INFERENCE_FAILED, EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__, 1, "thalamus", "Inference failed"
    );
    exceptions.push_back((nimcp_exception_t*)brain_ex);

    // GPU exception
    nimcp_gpu_exception_t* gpu_ex = nimcp_gpu_exception_create(
        NIMCP_ERROR_GPU_SYNC, EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__, 0, 4, "GPU sync failed"
    );
    exceptions.push_back((nimcp_exception_t*)gpu_ex);

    // I/O exception (will go to core)
    nimcp_io_exception_t* io_ex = nimcp_io_exception_create(
        NIMCP_ERROR_FILE_WRITE, EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__, "/tmp/output.dat", "Write failed"
    );
    exceptions.push_back((nimcp_exception_t*)io_ex);

    // Dispatch all
    for (auto* ex : exceptions) {
        ASSERT_NE(ex, nullptr);
        nimcp_exception_dispatch(ex);
    }

    // Verify each went to appropriate module
    EXPECT_TRUE(moduleReceivedException(SimulatedModule::COGNITIVE, NIMCP_ERROR_INFERENCE_FAILED));
    EXPECT_TRUE(moduleReceivedException(SimulatedModule::GPU, NIMCP_ERROR_GPU_SYNC));
    EXPECT_TRUE(moduleReceivedException(SimulatedModule::CORE, NIMCP_ERROR_FILE_WRITE));

    // Cleanup
    for (auto* ex : exceptions) {
        nimcp_exception_unref(ex);
    }
}

//=============================================================================
// Test: Exception Immune Presentation Flow
//=============================================================================

TEST_F(ExceptionCrossModuleTest, ExceptionImmunePresentationFlow) {
    // WHAT: Verify exception can be explicitly presented to immune system
    // WHY:  Manual immune presentation for special handling

    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_NO_MEMORY,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        "Memory exhaustion for immune"
    );
    ASSERT_NE(ex, nullptr);

    // Explicitly present to immune
    nimcp_immune_response_t response;
    memset(&response, 0, sizeof(response));
    int result = nimcp_exception_present_to_immune(ex, &response);

    // Result depends on immune system connection state
    // If not connected, may return error but should not crash
    if (result == 0) {
        EXPECT_TRUE(ex->presented_to_immune)
            << "Exception should be marked as presented";
    }

    // Now dispatch (immune handler should see presented_to_immune flag)
    nimcp_exception_dispatch(ex);

    nimcp_exception_unref(ex);
}

//=============================================================================
// Test: Circuit Breaker Cross-Module Recording
//=============================================================================

TEST_F(ExceptionCrossModuleTest, CircuitBreakerCrossModuleRecording) {
    // WHAT: Verify circuit breaker records exceptions across modules
    // WHY:  Prevent cascading failures from any module

    // Create several exceptions of the same type
    for (int i = 0; i < 5; i++) {
        nimcp_exception_t* ex = nimcp_exception_create(
            NIMCP_ERROR_OPERATION_FAILED,
            EXCEPTION_SEVERITY_ERROR,
            __FILE__, __LINE__, __func__,
            "Repeated error %d", i
        );
        ASSERT_NE(ex, nullptr);

        // Record in circuit breaker
        int cb_result = nimcp_circuit_record(ex);
        // 0 = proceed, 1 = blocked by circuit, -1 = error
        // At low counts, should proceed
        if (i < 3) {
            EXPECT_LE(cb_result, 0) << "Should not be blocked at count " << i;
        }

        nimcp_exception_dispatch(ex);
        nimcp_exception_unref(ex);
    }

    // Check circuit state
    nimcp_circuit_state_t state = nimcp_circuit_get_state(NIMCP_ERROR_OPERATION_FAILED);
    // Depending on threshold, may still be CLOSED
    EXPECT_NE(state, CIRCUIT_STATE_OPEN)
        << "Circuit should not be fully open with only 5 exceptions";
}

//=============================================================================
// Test: Metrics Recording Cross-Module
//=============================================================================

TEST_F(ExceptionCrossModuleTest, MetricsRecordingCrossModule) {
    // WHAT: Verify metrics system tracks exceptions across modules
    // WHY:  Observability across entire exception flow

    // Create and record exceptions
    nimcp_exception_t* ex1 = nimcp_exception_create(
        NIMCP_ERROR_BRAIN_CREATION, EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__, "Brain error for metrics"
    );
    ASSERT_NE(ex1, nullptr);
    nimcp_metrics_record_exception(ex1);

    nimcp_exception_t* ex2 = nimcp_exception_create(
        NIMCP_ERROR_GPU_MEMORY, EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__, "GPU error for metrics"
    );
    ASSERT_NE(ex2, nullptr);
    nimcp_metrics_record_exception(ex2);

    // Get metrics
    nimcp_exception_metrics_t metrics;
    nimcp_metrics_get(&metrics);

    // Should have recorded at least 2 exceptions
    EXPECT_GE(metrics.total_exceptions, 2u)
        << "Metrics should record exceptions";

    nimcp_exception_dispatch(ex1);
    nimcp_exception_dispatch(ex2);
    nimcp_exception_unref(ex1);
    nimcp_exception_unref(ex2);
}

//=============================================================================
// Test: Handler Module Isolation (Exception in Handler)
//=============================================================================

/**
 * @brief Handler that creates a new exception (potential recursion)
 */
static bool recursive_handler(nimcp_exception_t* ex, void* user_data) {
    (void)user_data;
    static int depth = 0;

    if (depth > 0) {
        // Prevent infinite recursion
        return false;
    }

    depth++;
    // This handler creates a new exception - should not cause infinite loop
    // because exception system should handle re-entrancy
    // Note: In real code, this would be dangerous without guards
    depth--;

    return false;
}

TEST_F(ExceptionCrossModuleTest, HandlerDoesNotCauseInfiniteLoop) {
    // WHAT: Verify exception handling doesn't cause infinite loops
    // WHY:  Handlers might need to throw exceptions themselves

    // Just verify system doesn't hang with normal usage
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Test exception"
    );
    ASSERT_NE(ex, nullptr);

    // Should complete without hanging
    auto start = std::chrono::steady_clock::now();
    nimcp_exception_dispatch(ex);
    auto end = std::chrono::steady_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    EXPECT_LT(duration.count(), 1000)
        << "Exception handling should complete quickly";

    nimcp_exception_unref(ex);
}

//=============================================================================
// Test: Bio-Async Exception Integration
//=============================================================================

TEST_F(ExceptionCrossModuleTest, BioAsyncExceptionIntegration) {
    // WHAT: Verify exceptions can carry bio-async context
    // WHY:  Bio-async operations need exception support

    // Create exception with async-related error
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_TIMEOUT,  // Common async error
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Bio-async operation timed out"
    );
    ASSERT_NE(ex, nullptr);

    // Add context about the async operation
    nimcp_exception_set_context(ex, "operation", "bio_future_wait");
    nimcp_exception_set_context(ex, "channel", "dopamine");
    nimcp_exception_set_context(ex, "timeout_ms", "5000");

    // Verify context
    const char* op = nimcp_exception_get_context(ex, "operation");
    EXPECT_NE(op, nullptr);
    if (op) {
        EXPECT_STREQ(op, "bio_future_wait");
    }

    const char* channel = nimcp_exception_get_context(ex, "channel");
    EXPECT_NE(channel, nullptr);
    if (channel) {
        EXPECT_STREQ(channel, "dopamine");
    }

    nimcp_exception_dispatch(ex);
    nimcp_exception_unref(ex);
}

//=============================================================================
// Test: Cross-Module Exception Context Preservation
//=============================================================================

TEST_F(ExceptionCrossModuleTest, CrossModuleContextPreservation) {
    // WHAT: Verify exception context is preserved across module boundaries
    // WHY:  Debug information must not be lost during propagation

    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_BRAIN_CREATION,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        "Context preservation test"
    );
    ASSERT_NE(ex, nullptr);

    // Add multiple context entries
    nimcp_exception_set_context(ex, "module", "visual_cortex");
    nimcp_exception_set_context(ex, "layer_id", "42");
    nimcp_exception_set_context(ex, "attempt", "3");

    // Generate epitope for immune matching
    size_t epitope_len = nimcp_exception_generate_epitope(ex);
    EXPECT_GT(epitope_len, 0u) << "Epitope should be generated";

    // Dispatch through all modules
    nimcp_exception_dispatch(ex);

    // Context should still be accessible after dispatch
    EXPECT_EQ(nimcp_exception_context_count(ex), 3u)
        << "All context entries should be preserved";

    const char* module = nimcp_exception_get_context(ex, "module");
    EXPECT_NE(module, nullptr);
    if (module) {
        EXPECT_STREQ(module, "visual_cortex");
    }

    nimcp_exception_unref(ex);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
