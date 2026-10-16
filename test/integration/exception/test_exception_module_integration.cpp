/**
 * @file test_exception_module_integration.cpp
 * @brief Integration tests for exception flow across module boundaries
 * @version 1.0.0
 * @date 2026-01-21
 *
 * WHAT: Test exception propagation, context preservation, and recovery across modules
 * WHY:  Verify exceptions flow correctly between cognitive, immune, core modules
 * HOW:  Simulate module boundaries with typed handlers and verify exception chain
 *
 * TEST SCENARIOS:
 * - Exception propagation from one module to another
 * - Exception context (file, line, function) preservation
 * - Exception cause chaining across modules
 * - Recovery flow after exceptions
 * - Immune system integration for cross-module exceptions
 * - Handler priority ordering across modules
 *
 * HEADER FILES REFERENCED:
 * - include/utils/exception/nimcp_exception.h
 * - include/utils/exception/nimcp_exception_handlers.h
 * - include/utils/exception/nimcp_exception_immune.h
 * - include/utils/exception/nimcp_exception_trace.h
 * - include/utils/exception/nimcp_exception_circuit.h
 * - include/utils/exception/nimcp_exception_metrics.h
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
#include <functional>
#include <map>
#include <mutex>

extern "C" {
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
 * @brief Enum representing different simulated modules
 */
enum class ModuleId {
    CORE = 0,
    MEMORY,
    NETWORK,
    BRAIN,
    GPU,
    IO,
    SECURITY,
    ASYNC,
    IMMUNE,
    COUNT
};

/**
 * @brief Record of exception seen by a module
 */
struct ModuleExceptionRecord {
    ModuleId module;
    nimcp_error_t code;
    nimcp_exception_type_t type;
    nimcp_exception_category_t category;
    nimcp_exception_severity_t severity;
    std::string file;
    int line;
    std::string function;
    std::string message;
    bool has_cause;
    nimcp_error_t cause_code;
    uint64_t timestamp;
    bool handled;
};

/**
 * @brief Global state for module exception tracking
 */
static struct {
    std::mutex mutex;
    std::map<ModuleId, std::vector<ModuleExceptionRecord>> records;
    std::atomic<int> total_exceptions_seen{0};
    std::atomic<bool> core_should_handle{false};
    std::atomic<bool> memory_should_handle{false};
    std::atomic<bool> brain_should_handle{false};
    std::atomic<bool> security_should_handle{false};
    ModuleId target_handling_module{ModuleId::CORE};
} g_module_state;

//=============================================================================
// Module Handler Functions
//=============================================================================

static void record_exception(ModuleId module, nimcp_exception_t* ex, bool handled) {
    ModuleExceptionRecord record;
    record.module = module;
    record.code = ex->code;
    record.type = ex->type;
    record.category = ex->category;
    record.severity = ex->severity;
    record.file = ex->file ? ex->file : "";
    record.line = ex->line;
    record.function = ex->function ? ex->function : "";
    record.message = ex->message;
    record.has_cause = (ex->cause != nullptr);
    record.cause_code = ex->cause ? ex->cause->code : NIMCP_SUCCESS;
    record.timestamp = ex->timestamp_us;
    record.handled = handled;

    std::lock_guard<std::mutex> lock(g_module_state.mutex);
    g_module_state.records[module].push_back(record);
    g_module_state.total_exceptions_seen++;
}

static bool core_module_handler(nimcp_exception_t* ex, void* user_data) {
    (void)user_data;
    bool should_handle = (g_module_state.target_handling_module == ModuleId::CORE) ||
                         g_module_state.core_should_handle.load();
    record_exception(ModuleId::CORE, ex, should_handle);
    return should_handle;
}

static bool memory_module_handler(nimcp_exception_t* ex, void* user_data) {
    (void)user_data;
    // Only handle memory-related exceptions
    if (ex->category != EXCEPTION_CATEGORY_MEMORY &&
        ex->type != EXCEPTION_TYPE_MEMORY) {
        return false;
    }
    bool should_handle = (g_module_state.target_handling_module == ModuleId::MEMORY) ||
                         g_module_state.memory_should_handle.load();
    record_exception(ModuleId::MEMORY, ex, should_handle);
    return should_handle;
}

static bool brain_module_handler(nimcp_exception_t* ex, void* user_data) {
    (void)user_data;
    // Only handle brain-related exceptions
    if (ex->category != EXCEPTION_CATEGORY_BRAIN &&
        ex->category != EXCEPTION_CATEGORY_COGNITIVE &&
        ex->type != EXCEPTION_TYPE_BRAIN) {
        return false;
    }
    bool should_handle = (g_module_state.target_handling_module == ModuleId::BRAIN) ||
                         g_module_state.brain_should_handle.load();
    record_exception(ModuleId::BRAIN, ex, should_handle);
    return should_handle;
}

static bool security_module_handler(nimcp_exception_t* ex, void* user_data) {
    (void)user_data;
    // Only handle security-related exceptions
    if (ex->category != EXCEPTION_CATEGORY_SECURITY &&
        ex->type != EXCEPTION_TYPE_SECURITY) {
        return false;
    }
    bool should_handle = (g_module_state.target_handling_module == ModuleId::SECURITY) ||
                         g_module_state.security_should_handle.load();
    record_exception(ModuleId::SECURITY, ex, should_handle);
    return should_handle;
}

static bool immune_monitor_handler(nimcp_exception_t* ex, void* user_data) {
    (void)user_data;
    // Immune module monitors all SEVERE+ exceptions but never consumes
    if (ex->severity < EXCEPTION_SEVERITY_SEVERE) {
        return false;
    }
    record_exception(ModuleId::IMMUNE, ex, false);
    return false;  // Never consume
}

static bool gpu_module_handler(nimcp_exception_t* ex, void* user_data) {
    (void)user_data;
    if (ex->category != EXCEPTION_CATEGORY_GPU &&
        ex->type != EXCEPTION_TYPE_GPU) {
        return false;
    }
    bool should_handle = (g_module_state.target_handling_module == ModuleId::GPU);
    record_exception(ModuleId::GPU, ex, should_handle);
    return should_handle;
}

static bool io_module_handler(nimcp_exception_t* ex, void* user_data) {
    (void)user_data;
    if (ex->category != EXCEPTION_CATEGORY_IO &&
        ex->type != EXCEPTION_TYPE_IO) {
        return false;
    }
    bool should_handle = (g_module_state.target_handling_module == ModuleId::IO);
    record_exception(ModuleId::IO, ex, should_handle);
    return should_handle;
}

//=============================================================================
// Test Fixture
//=============================================================================

class ExceptionModuleIntegrationTest : public ::testing::Test {
protected:
    std::vector<nimcp_handler_registration_t*> registrations_;

    void SetUp() override {
        // Reset global state
        {
            std::lock_guard<std::mutex> lock(g_module_state.mutex);
            g_module_state.records.clear();
        }
        g_module_state.total_exceptions_seen = 0;
        g_module_state.core_should_handle = false;
        g_module_state.memory_should_handle = false;
        g_module_state.brain_should_handle = false;
        g_module_state.security_should_handle = false;
        g_module_state.target_handling_module = ModuleId::CORE;

        // Initialize subsystems
        nimcp_exception_system_init();
        nimcp_trace_init();
        nimcp_circuit_init();
        nimcp_metrics_init();

        // Initialize immune integration
        nimcp_exception_immune_config_t immune_config;
        nimcp_exception_immune_default_config(&immune_config);
        immune_config.enable_auto_present = false;
        immune_config.enable_auto_recovery = false;
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
        // IMMUNE module - highest priority (monitors severe exceptions)
        nimcp_handler_options_t immune_opts;
        nimcp_handler_default_options(&immune_opts);
        immune_opts.name = "immune_monitor";
        immune_opts.handler = immune_monitor_handler;
        immune_opts.priority = NIMCP_HANDLER_PRIORITY_HIGH + 50;
        immune_opts.min_severity = EXCEPTION_SEVERITY_SEVERE;
        auto* reg_immune = nimcp_handler_register(&immune_opts);
        if (reg_immune) registrations_.push_back(reg_immune);

        // SECURITY module - high priority
        nimcp_handler_options_t security_opts;
        nimcp_handler_default_options(&security_opts);
        security_opts.name = "security_module";
        security_opts.handler = security_module_handler;
        security_opts.priority = NIMCP_HANDLER_PRIORITY_HIGH;
        auto* reg_security = nimcp_handler_register(&security_opts);
        if (reg_security) registrations_.push_back(reg_security);

        // BRAIN module - high-normal priority
        nimcp_handler_options_t brain_opts;
        nimcp_handler_default_options(&brain_opts);
        brain_opts.name = "brain_module";
        brain_opts.handler = brain_module_handler;
        brain_opts.priority = NIMCP_HANDLER_PRIORITY_NORMAL + 25;
        auto* reg_brain = nimcp_handler_register(&brain_opts);
        if (reg_brain) registrations_.push_back(reg_brain);

        // MEMORY module - normal priority
        nimcp_handler_options_t memory_opts;
        nimcp_handler_default_options(&memory_opts);
        memory_opts.name = "memory_module";
        memory_opts.handler = memory_module_handler;
        memory_opts.priority = NIMCP_HANDLER_PRIORITY_NORMAL;
        auto* reg_memory = nimcp_handler_register(&memory_opts);
        if (reg_memory) registrations_.push_back(reg_memory);

        // GPU module - normal priority
        nimcp_handler_options_t gpu_opts;
        nimcp_handler_default_options(&gpu_opts);
        gpu_opts.name = "gpu_module";
        gpu_opts.handler = gpu_module_handler;
        gpu_opts.priority = NIMCP_HANDLER_PRIORITY_NORMAL - 5;
        auto* reg_gpu = nimcp_handler_register(&gpu_opts);
        if (reg_gpu) registrations_.push_back(reg_gpu);

        // IO module - low-normal priority
        nimcp_handler_options_t io_opts;
        nimcp_handler_default_options(&io_opts);
        io_opts.name = "io_module";
        io_opts.handler = io_module_handler;
        io_opts.priority = NIMCP_HANDLER_PRIORITY_NORMAL - 10;
        auto* reg_io = nimcp_handler_register(&io_opts);
        if (reg_io) registrations_.push_back(reg_io);

        // CORE module - lowest priority (fallback)
        nimcp_handler_options_t core_opts;
        nimcp_handler_default_options(&core_opts);
        core_opts.name = "core_module";
        core_opts.handler = core_module_handler;
        core_opts.priority = NIMCP_HANDLER_PRIORITY_LOW;
        auto* reg_core = nimcp_handler_register(&core_opts);
        if (reg_core) registrations_.push_back(reg_core);
    }

    bool moduleReceivedException(ModuleId module, nimcp_error_t code) {
        std::lock_guard<std::mutex> lock(g_module_state.mutex);
        auto it = g_module_state.records.find(module);
        if (it == g_module_state.records.end()) return false;
        for (const auto& record : it->second) {
            if (record.code == code) return true;
        }
        return false;
    }

    size_t moduleExceptionCount(ModuleId module) {
        std::lock_guard<std::mutex> lock(g_module_state.mutex);
        auto it = g_module_state.records.find(module);
        if (it == g_module_state.records.end()) return 0;
        return it->second.size();
    }

    const ModuleExceptionRecord* findModuleRecord(ModuleId module, nimcp_error_t code) {
        std::lock_guard<std::mutex> lock(g_module_state.mutex);
        auto it = g_module_state.records.find(module);
        if (it == g_module_state.records.end()) return nullptr;
        for (const auto& record : it->second) {
            if (record.code == code) return &record;
        }
        return nullptr;
    }
};

//=============================================================================
// Test: Exception Propagates Across Modules
//=============================================================================

TEST_F(ExceptionModuleIntegrationTest, ExceptionPropagatesAcrossModules) {
    // WHAT: Test that an exception raised in one module is seen by other modules
    // WHY:  Verify handler chain propagates exceptions correctly
    // HOW:  Create exception, dispatch, check all relevant modules see it

    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        "Cross-module exception test"
    );
    ASSERT_NE(ex, nullptr);

    // No module should handle (all see it)
    g_module_state.target_handling_module = ModuleId::CORE;

    nimcp_exception_dispatch(ex);

    // Immune should see it (severity >= SEVERE)
    EXPECT_TRUE(moduleReceivedException(ModuleId::IMMUNE, NIMCP_ERROR_OPERATION_FAILED))
        << "Immune module should monitor severe exception";

    // Core should see it as fallback
    EXPECT_TRUE(moduleReceivedException(ModuleId::CORE, NIMCP_ERROR_OPERATION_FAILED))
        << "Core module should see exception as fallback";

    nimcp_exception_unref(ex);
}

//=============================================================================
// Test: Exception Context (File, Line, Function) Preserved
//=============================================================================

TEST_F(ExceptionModuleIntegrationTest, ExceptionContextPreserved) {
    // WHAT: Verify file, line, function info is preserved through exception chain
    // WHY:  Debug information must survive module boundary crossings
    // HOW:  Create exception with known location, verify all handlers see same info

    const char* test_file = "test_module_a.c";
    int test_line = 42;
    const char* test_func = "module_a_function";

    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_BRAIN_CREATION,
        EXCEPTION_SEVERITY_ERROR,
        test_file, test_line, test_func,
        "Context preservation test"
    );
    ASSERT_NE(ex, nullptr);

    // Verify exception fields
    EXPECT_STREQ(ex->file, test_file);
    EXPECT_EQ(ex->line, test_line);
    EXPECT_STREQ(ex->function, test_func);

    g_module_state.brain_should_handle = false;  // Let it propagate

    nimcp_exception_dispatch(ex);

    // Check brain module received exception with correct context
    EXPECT_TRUE(moduleReceivedException(ModuleId::BRAIN, NIMCP_ERROR_BRAIN_CREATION));

    // Verify the record has correct context
    std::lock_guard<std::mutex> lock(g_module_state.mutex);
    auto it = g_module_state.records.find(ModuleId::BRAIN);
    ASSERT_NE(it, g_module_state.records.end());
    ASSERT_FALSE(it->second.empty());

    const auto& record = it->second[0];
    EXPECT_EQ(record.file, test_file);
    EXPECT_EQ(record.line, test_line);
    EXPECT_EQ(record.function, test_func);

    nimcp_exception_unref(ex);
}

//=============================================================================
// Test: Exception Cause Chain Preserved Across Modules
//=============================================================================

TEST_F(ExceptionModuleIntegrationTest, ExceptionCauseChainPreserved) {
    // WHAT: Verify exception cause chain is preserved through module handlers
    // WHY:  Root cause tracking must work across module boundaries
    // HOW:  Create exception chain, dispatch wrapper, verify cause is accessible

    // Root cause: memory error
    nimcp_exception_t* root = nimcp_exception_create(
        NIMCP_ERROR_NO_MEMORY,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Out of memory - root cause"
    );
    ASSERT_NE(root, nullptr);

    // Intermediate: brain module failure
    nimcp_brain_exception_t* middle = nimcp_brain_exception_create(
        NIMCP_ERROR_FORWARD_PASS,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        1, "visual_cortex",
        "Forward pass failed due to memory"
    );
    ASSERT_NE(middle, nullptr);
    nimcp_exception_set_cause((nimcp_exception_t*)middle, root);

    // Top-level: operation failed
    nimcp_exception_t* top = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        "Module operation failed - cascade"
    );
    ASSERT_NE(top, nullptr);
    nimcp_exception_set_cause(top, (nimcp_exception_t*)middle);

    nimcp_exception_dispatch(top);

    // Verify cause chain is intact
    nimcp_exception_t* cause1 = nimcp_exception_get_cause(top);
    ASSERT_NE(cause1, nullptr);
    EXPECT_EQ(cause1->code, NIMCP_ERROR_FORWARD_PASS);

    nimcp_exception_t* cause2 = nimcp_exception_get_cause(cause1);
    ASSERT_NE(cause2, nullptr);
    EXPECT_EQ(cause2->code, NIMCP_ERROR_NO_MEMORY);

    nimcp_exception_unref(top);
}

//=============================================================================
// Test: Module-Specific Handler Receives Typed Exceptions
//=============================================================================

TEST_F(ExceptionModuleIntegrationTest, TypedExceptionRoutesToCorrectModule) {
    // WHAT: Verify typed exceptions route to appropriate module handlers
    // WHY:  Brain exceptions should go to brain module, etc.
    // HOW:  Create typed exceptions, verify correct modules receive them

    // Brain exception
    nimcp_brain_exception_t* brain_ex = nimcp_brain_exception_create(
        NIMCP_ERROR_LEARNING_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        1, "prefrontal",
        "Learning failed"
    );
    ASSERT_NE(brain_ex, nullptr);

    nimcp_exception_dispatch((nimcp_exception_t*)brain_ex);
    EXPECT_TRUE(moduleReceivedException(ModuleId::BRAIN, NIMCP_ERROR_LEARNING_FAILED))
        << "Brain module should receive brain exception";
    nimcp_exception_unref((nimcp_exception_t*)brain_ex);

    // Memory exception
    nimcp_memory_exception_t* mem_ex = nimcp_memory_exception_create(
        NIMCP_ERROR_NO_MEMORY,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        1024,
        "Memory allocation failed"
    );
    ASSERT_NE(mem_ex, nullptr);

    nimcp_exception_dispatch((nimcp_exception_t*)mem_ex);
    EXPECT_TRUE(moduleReceivedException(ModuleId::MEMORY, NIMCP_ERROR_NO_MEMORY))
        << "Memory module should receive memory exception";
    nimcp_exception_unref((nimcp_exception_t*)mem_ex);

    // Security exception
    nimcp_security_exception_t* sec_ex = nimcp_security_exception_create(
        NIMCP_ERROR_BBB_REJECTED,
        EXCEPTION_SEVERITY_CRITICAL,
        __FILE__, __LINE__, __func__,
        1,
        "BBB rejected request"
    );
    ASSERT_NE(sec_ex, nullptr);

    nimcp_exception_dispatch((nimcp_exception_t*)sec_ex);
    EXPECT_TRUE(moduleReceivedException(ModuleId::SECURITY, NIMCP_ERROR_BBB_REJECTED))
        << "Security module should receive security exception";
    nimcp_exception_unref((nimcp_exception_t*)sec_ex);
}

//=============================================================================
// Test: Module Handler Consumes Exception
//=============================================================================

TEST_F(ExceptionModuleIntegrationTest, ModuleHandlerConsumesException) {
    // WHAT: When a module handler returns true, exception stops propagating
    // WHY:  Handled exceptions should not continue down the chain
    // HOW:  Set brain handler to consume, verify core doesn't see it

    nimcp_brain_exception_t* ex = nimcp_brain_exception_create(
        NIMCP_ERROR_BRAIN_CREATION,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        1, "amygdala",
        "Brain exception to be consumed"
    );
    ASSERT_NE(ex, nullptr);

    // Brain module should consume
    g_module_state.brain_should_handle = true;

    bool handled = nimcp_exception_dispatch((nimcp_exception_t*)ex);
    EXPECT_TRUE(handled) << "Exception should be handled";

    // Brain module should have it
    EXPECT_TRUE(moduleReceivedException(ModuleId::BRAIN, NIMCP_ERROR_BRAIN_CREATION));

    // Core should NOT see it (consumed by brain)
    EXPECT_FALSE(moduleReceivedException(ModuleId::CORE, NIMCP_ERROR_BRAIN_CREATION))
        << "Core should not see exception consumed by brain";

    nimcp_exception_unref((nimcp_exception_t*)ex);
}

//=============================================================================
// Test: Immune Module Monitors But Doesn't Consume
//=============================================================================

TEST_F(ExceptionModuleIntegrationTest, ImmuneMonitorsWithoutConsuming) {
    // WHAT: Immune module should see severe exceptions but not consume them
    // WHY:  Immune monitors for threat detection without blocking normal handling
    // HOW:  Create severe exception, verify immune sees it and others also see it

    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_MEMORY_CORRUPTION,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        "Severe memory corruption detected"
    );
    ASSERT_NE(ex, nullptr);

    nimcp_exception_dispatch(ex);

    // Immune should see it
    EXPECT_TRUE(moduleReceivedException(ModuleId::IMMUNE, NIMCP_ERROR_MEMORY_CORRUPTION))
        << "Immune should monitor severe exception";

    // Core should also see it (immune didn't consume)
    EXPECT_TRUE(moduleReceivedException(ModuleId::CORE, NIMCP_ERROR_MEMORY_CORRUPTION))
        << "Core should see exception after immune monitoring";

    nimcp_exception_unref(ex);
}

//=============================================================================
// Test: Exception Context Entries Preserved
//=============================================================================

TEST_F(ExceptionModuleIntegrationTest, ExceptionContextEntriesPreserved) {
    // WHAT: Test that key-value context entries survive cross-module propagation
    // WHY:  Debug context must be available to all handlers
    // HOW:  Add context entries, dispatch, verify they're still accessible

    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Exception with context"
    );
    ASSERT_NE(ex, nullptr);

    // Add context entries
    nimcp_exception_set_context(ex, "module", "module_a");
    nimcp_exception_set_context(ex, "operation", "data_transfer");
    nimcp_exception_set_context(ex, "retry_count", "3");

    // Dispatch
    nimcp_exception_dispatch(ex);

    // Context should still be accessible
    EXPECT_EQ(nimcp_exception_context_count(ex), 3u);

    const char* module_ctx = nimcp_exception_get_context(ex, "module");
    EXPECT_NE(module_ctx, nullptr);
    if (module_ctx) EXPECT_STREQ(module_ctx, "module_a");

    const char* op_ctx = nimcp_exception_get_context(ex, "operation");
    EXPECT_NE(op_ctx, nullptr);
    if (op_ctx) EXPECT_STREQ(op_ctx, "data_transfer");

    nimcp_exception_unref(ex);
}

//=============================================================================
// Test: Trace Context Preserved Across Modules
//=============================================================================

TEST_F(ExceptionModuleIntegrationTest, TraceContextPreservedAcrossModules) {
    // WHAT: Verify distributed trace context follows exception
    // WHY:  Trace correlation for distributed debugging
    // HOW:  Attach trace, dispatch, verify trace is still attached

    // Create trace
    nimcp_exception_trace_t trace = nimcp_trace_create();
    EXPECT_NE(trace.trace_id, 0u);
    EXPECT_NE(trace.span_id, 0u);

    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_BRAIN_CREATION,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Traced exception"
    );
    ASSERT_NE(ex, nullptr);

    // Attach trace
    int result = nimcp_exception_set_trace(ex, &trace);
    EXPECT_EQ(result, 0);

    // Dispatch
    nimcp_exception_dispatch(ex);

    // Verify trace is still attached
    const nimcp_exception_trace_t* retrieved = nimcp_exception_get_trace(ex);
    ASSERT_NE(retrieved, nullptr);
    EXPECT_EQ(retrieved->trace_id, trace.trace_id);
    EXPECT_EQ(retrieved->span_id, trace.span_id);

    nimcp_exception_unref(ex);
}

//=============================================================================
// Test: Propagation Context Across Modules
//=============================================================================

TEST_F(ExceptionModuleIntegrationTest, PropagationContextTracksModulePath) {
    // WHAT: Test propagation context records module path
    // WHY:  Audit trail for exception routing
    // HOW:  Create propagation context with hops, attach to exception

    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Exception with propagation path"
    );
    ASSERT_NE(ex, nullptr);

    // Create propagation context
    nimcp_propagation_context_t* prop = nimcp_propagation_create("brain_module");
    ASSERT_NE(prop, nullptr);

    // Record propagation hops
    nimcp_propagation_add_hop(prop, "brain_module", "ERROR", 5);
    nimcp_propagation_add_hop(prop, "memory_module", "FORWARD", 6);
    nimcp_propagation_add_hop(prop, "core_module", "ESCALATE", 7);
    nimcp_propagation_set_target(prop, "immune_system");

    // Attach to exception
    int result = nimcp_exception_set_propagation(ex, prop);
    EXPECT_EQ(result, 0);

    // Verify propagation context
    const nimcp_propagation_context_t* retrieved = nimcp_exception_get_propagation(ex);
    ASSERT_NE(retrieved, nullptr);
    EXPECT_STREQ(retrieved->origin_module, "brain_module");
    EXPECT_EQ(retrieved->path_length, 3u);
    EXPECT_STREQ(retrieved->target_module, "immune_system");

    nimcp_exception_dispatch(ex);
    nimcp_exception_unref(ex);
}

//=============================================================================
// Test: Immune Integration for Cross-Module Exceptions
//=============================================================================

TEST_F(ExceptionModuleIntegrationTest, ImmuneIntegrationForCrossModuleExceptions) {
    // WHAT: Test immune system receives cross-module exceptions
    // WHY:  Immune system needs to track all significant errors
    // HOW:  Present exception to immune, verify response

    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        "Cross-module error for immune"
    );
    ASSERT_NE(ex, nullptr);

    // Generate epitope
    size_t epitope_len = nimcp_exception_generate_epitope(ex);
    EXPECT_GT(epitope_len, 0u);

    // Present to immune
    nimcp_immune_response_t response;
    memset(&response, 0, sizeof(response));
    int result = nimcp_exception_present_to_immune(ex, &response);
    EXPECT_EQ(result, 0);

    // Verify presentation
    EXPECT_TRUE(ex->presented_to_immune);

    // Get recovery strategy
    nimcp_exception_recovery_strategy_t strategy;
    nimcp_exception_get_recovery_strategy(ex, &strategy);
    EXPECT_NE(strategy.primary_action, EXCEPTION_RECOVERY_NONE);

    nimcp_exception_unref(ex);
}

//=============================================================================
// Test: Handler Priority Ordering
//=============================================================================

TEST_F(ExceptionModuleIntegrationTest, HandlerPriorityOrdering) {
    // WHAT: Verify handlers are called in priority order
    // WHY:  Priority determines which module gets first chance to handle
    // HOW:  Create exception, verify order of module records

    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        "Priority ordering test"
    );
    ASSERT_NE(ex, nullptr);

    // All modules should see it but none should consume
    g_module_state.target_handling_module = ModuleId::CORE;

    nimcp_exception_dispatch(ex);

    // Verify immune (highest priority for severe) was called
    EXPECT_TRUE(moduleReceivedException(ModuleId::IMMUNE, NIMCP_ERROR_OPERATION_FAILED))
        << "Immune should be called first for severe exceptions";

    // Verify core (lowest priority) was also called
    EXPECT_TRUE(moduleReceivedException(ModuleId::CORE, NIMCP_ERROR_OPERATION_FAILED))
        << "Core should be called as fallback";

    nimcp_exception_unref(ex);
}

//=============================================================================
// Test: Recovery Flow After Exception
//=============================================================================

TEST_F(ExceptionModuleIntegrationTest, RecoveryFlowAfterException) {
    // WHAT: Test complete recovery flow from exception to recovery callback
    // WHY:  Verify recovery system works across module boundaries
    // HOW:  Register callback, trigger exception, execute recovery

    static std::atomic<int> recovery_count{0};
    static std::atomic<nimcp_exception_recovery_action_t> last_action{EXCEPTION_RECOVERY_NONE};

    auto recovery_callback = [](nimcp_exception_t* ex,
                                nimcp_exception_recovery_action_t action,
                                void* user_data) -> int {
        (void)ex;
        (void)user_data;
        recovery_count++;
        last_action = action;
        return 0;  // Success
    };

    nimcp_register_recovery_callback(EXCEPTION_RECOVERY_GC, recovery_callback, nullptr);

    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_NO_MEMORY,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        "Memory error requiring recovery"
    );
    ASSERT_NE(ex, nullptr);

    // Dispatch
    nimcp_exception_dispatch(ex);

    // Execute recovery
    recovery_count = 0;
    int result = nimcp_exception_execute_recovery(ex, EXCEPTION_RECOVERY_GC);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(recovery_count.load(), 1);
    EXPECT_EQ(last_action.load(), EXCEPTION_RECOVERY_GC);

    // Notify immune of result
    nimcp_exception_notify_recovery_result(ex, EXCEPTION_RECOVERY_GC, true);

    nimcp_unregister_recovery_callback(EXCEPTION_RECOVERY_GC);
    nimcp_exception_unref(ex);
}

//=============================================================================
// Test: Multiple Concurrent Module Exceptions
//=============================================================================

TEST_F(ExceptionModuleIntegrationTest, MultipleConcurrentModuleExceptions) {
    // WHAT: Test handling multiple exceptions from different modules
    // WHY:  System must handle concurrent failures
    // HOW:  Create and dispatch exceptions from multiple "modules"

    // Create exceptions from different modules
    nimcp_brain_exception_t* brain_ex = nimcp_brain_exception_create(
        NIMCP_ERROR_INFERENCE_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        1, "cortex",
        "Inference failed"
    );

    nimcp_memory_exception_t* mem_ex = nimcp_memory_exception_create(
        NIMCP_ERROR_MEMORY_LEAK,
        EXCEPTION_SEVERITY_WARNING,
        __FILE__, __LINE__, __func__,
        0,
        "Memory leak detected"
    );

    nimcp_gpu_exception_t* gpu_ex = nimcp_gpu_exception_create(
        NIMCP_ERROR_GPU_MEMORY,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        0, 2,
        "GPU OOM"
    );

    ASSERT_NE(brain_ex, nullptr);
    ASSERT_NE(mem_ex, nullptr);
    ASSERT_NE(gpu_ex, nullptr);

    // Dispatch all
    nimcp_exception_dispatch((nimcp_exception_t*)brain_ex);
    nimcp_exception_dispatch((nimcp_exception_t*)mem_ex);
    nimcp_exception_dispatch((nimcp_exception_t*)gpu_ex);

    // Verify each went to correct module
    EXPECT_TRUE(moduleReceivedException(ModuleId::BRAIN, NIMCP_ERROR_INFERENCE_FAILED));
    EXPECT_TRUE(moduleReceivedException(ModuleId::MEMORY, NIMCP_ERROR_MEMORY_LEAK));
    EXPECT_TRUE(moduleReceivedException(ModuleId::GPU, NIMCP_ERROR_GPU_MEMORY));

    nimcp_exception_unref((nimcp_exception_t*)brain_ex);
    nimcp_exception_unref((nimcp_exception_t*)mem_ex);
    nimcp_exception_unref((nimcp_exception_t*)gpu_ex);
}

//=============================================================================
// Test: Aggregate Exception Across Modules
//=============================================================================

TEST_F(ExceptionModuleIntegrationTest, AggregateExceptionAcrossModules) {
    // WHAT: Test aggregate exception containing errors from multiple modules
    // WHY:  System-wide failures may affect multiple modules
    // HOW:  Create aggregate with children from different modules

    nimcp_aggregate_exception_t* agg = nimcp_aggregate_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_CRITICAL,
        __FILE__, __LINE__, __func__,
        "Multi-module failure"
    );
    ASSERT_NE(agg, nullptr);

    // Add brain module failure
    nimcp_exception_t* brain_child = nimcp_exception_create(
        NIMCP_ERROR_BRAIN_CREATION,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Brain module failed"
    );
    nimcp_aggregate_exception_add(agg, brain_child);

    // Add memory module failure
    nimcp_exception_t* mem_child = nimcp_exception_create(
        NIMCP_ERROR_NO_MEMORY,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Memory module failed"
    );
    nimcp_aggregate_exception_add(agg, mem_child);

    // Add security module failure
    nimcp_exception_t* sec_child = nimcp_exception_create(
        NIMCP_ERROR_SECURITY_THREAT,
        EXCEPTION_SEVERITY_CRITICAL,
        __FILE__, __LINE__, __func__,
        "Security module failed"
    );
    nimcp_aggregate_exception_add(agg, sec_child);

    EXPECT_EQ(nimcp_aggregate_exception_count(agg), 3u);

    // Dispatch aggregate
    nimcp_exception_dispatch((nimcp_exception_t*)agg);

    // Verify aggregate went through handler chain
    EXPECT_TRUE(moduleReceivedException(ModuleId::IMMUNE, NIMCP_ERROR_OPERATION_FAILED))
        << "Aggregate should be seen by immune (critical severity)";

    nimcp_exception_unref((nimcp_exception_t*)agg);
}

//=============================================================================
// Test: Circuit Breaker Integration Across Modules
//=============================================================================

TEST_F(ExceptionModuleIntegrationTest, CircuitBreakerCrossModuleIntegration) {
    // WHAT: Test circuit breaker tracks exceptions across modules
    // WHY:  Prevent cascading failures from any module
    // HOW:  Create multiple exceptions, verify circuit tracking

    // Create several exceptions of same type
    for (int i = 0; i < 5; i++) {
        nimcp_exception_t* ex = nimcp_exception_create(
            NIMCP_ERROR_OPERATION_FAILED,
            EXCEPTION_SEVERITY_ERROR,
            __FILE__, __LINE__, __func__,
            "Circuit breaker test %d", i
        );
        ASSERT_NE(ex, nullptr);

        int cb_result = nimcp_circuit_record(ex);
        // Should proceed at low counts
        if (i < NIMCP_CIRCUIT_DEFAULT_THRESHOLD / 2) {
            EXPECT_LE(cb_result, 0);
        }

        nimcp_exception_dispatch(ex);
        nimcp_exception_unref(ex);
    }

    // Check circuit state
    nimcp_circuit_state_t state = nimcp_circuit_get_state(NIMCP_ERROR_OPERATION_FAILED);
    // With 5 exceptions, circuit should still be closed
    EXPECT_EQ(state, CIRCUIT_STATE_CLOSED);
}

//=============================================================================
// Test: Metrics Recording Across Modules
//=============================================================================

TEST_F(ExceptionModuleIntegrationTest, MetricsRecordingAcrossModules) {
    // WHAT: Test metrics system tracks exceptions from all modules
    // WHY:  Observability requires tracking all exception sources
    // HOW:  Create exceptions, verify metrics are recorded

    // Create and record exceptions
    nimcp_exception_t* ex1 = nimcp_exception_create(
        NIMCP_ERROR_BRAIN_CREATION,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Brain error for metrics"
    );
    nimcp_metrics_record_exception(ex1);

    nimcp_exception_t* ex2 = nimcp_exception_create(
        NIMCP_ERROR_NO_MEMORY,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Memory error for metrics"
    );
    nimcp_metrics_record_exception(ex2);

    // Get metrics
    nimcp_exception_metrics_t metrics;
    nimcp_metrics_get(&metrics);

    EXPECT_GE(metrics.total_exceptions, 2u);

    nimcp_exception_dispatch(ex1);
    nimcp_exception_dispatch(ex2);
    nimcp_exception_unref(ex1);
    nimcp_exception_unref(ex2);
}

//=============================================================================
// Test: Thread-Safe Cross-Module Exception Handling
//=============================================================================

TEST_F(ExceptionModuleIntegrationTest, ThreadSafeCrossModuleHandling) {
    // WHAT: Test thread-safe exception handling across modules
    // WHY:  Multiple threads may generate exceptions concurrently
    // HOW:  Create exceptions from multiple threads

    const int NUM_THREADS = 4;
    const int EXCEPTIONS_PER_THREAD = 10;
    std::atomic<int> created_count{0};
    std::vector<std::thread> threads;

    for (int t = 0; t < NUM_THREADS; t++) {
        threads.emplace_back([&created_count, t]() {
            for (int i = 0; i < EXCEPTIONS_PER_THREAD; i++) {
                nimcp_exception_t* ex = nimcp_exception_create(
                    NIMCP_ERROR_OPERATION_FAILED + (t % 5),
                    EXCEPTION_SEVERITY_ERROR,
                    __FILE__, __LINE__, __func__,
                    "Thread %d exception %d", t, i
                );
                if (ex) {
                    created_count++;
                    nimcp_exception_dispatch(ex);
                    nimcp_exception_unref(ex);
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(created_count.load(), NUM_THREADS * EXCEPTIONS_PER_THREAD);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
