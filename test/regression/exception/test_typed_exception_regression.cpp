/**
 * @file test_typed_exception_regression.cpp
 * @brief Regression tests for typed exception API stability
 *
 * WHAT: Verify typed exception API contracts remain stable
 * WHY:  Prevent breaking changes to typed exception interfaces
 * HOW:  Test typed exception creation, field access, and macro behavior
 *
 * REGRESSION CATEGORIES:
 * 1. Typed Exception Creation - Creation functions must work correctly
 * 2. Field Assignment - Type-specific fields must be set correctly
 * 3. C Polymorphism - Base casting must work for all derived types
 * 4. Macro Integration - Typed throw macros must create correct types
 * 5. Memory Layout - Base struct must be first member
 * 6. Aggregate Exceptions - Aggregate operations must be stable
 *
 * HEADER FILES READ:
 * - nimcp_exception.h: Defines typed exception structs:
 *   - nimcp_memory_exception_t (requested_size, available_size, failed_address)
 *   - nimcp_brain_exception_t (brain_id, network_id, region_name)
 *   - nimcp_io_exception_t (path, errno_value, bytes_transferred)
 *   - nimcp_threading_exception_t (thread_id, thread_name, mutex_address)
 *   - nimcp_security_exception_t (threat_type, source_node_id, threat_signature)
 *   - nimcp_gpu_exception_t (device_id, cuda_error, kernel_name)
 *   - nimcp_signal_exception_t (signal_number, fault_address)
 *   - nimcp_aggregate_exception_t (children array)
 * - nimcp_exception_macros.h: Defines NIMCP_THROW_MEMORY, NIMCP_THROW_BRAIN, etc.
 *
 * EXACT FUNCTION SIGNATURES:
 * - nimcp_memory_exception_create(code, severity, file, line, func, requested_size, format, ...)
 * - nimcp_brain_exception_create(code, severity, file, line, func, brain_id, region_name, format, ...)
 * - nimcp_io_exception_create(code, severity, file, line, func, path, format, ...)
 * - nimcp_threading_exception_create(code, severity, file, line, func, thread_id, format, ...)
 * - nimcp_security_exception_create(code, severity, file, line, func, threat_type, format, ...)
 * - nimcp_gpu_exception_create(code, severity, file, line, func, device_id, cuda_error, format, ...)
 * - nimcp_signal_exception_create(signal_number, fault_address, file, line, func, format, ...)
 * - nimcp_aggregate_exception_create(code, severity, file, line, func, format, ...)
 *
 * @author NIMCP Development Team
 * @date 2026-01-21
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cstdint>

extern "C" {
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_handlers.h"
#include "utils/error/nimcp_error_codes.h"
}

//=============================================================================
// Test Fixture
//=============================================================================

class TypedExceptionRegressionTest : public ::testing::Test {
protected:
    void SetUp() override {
        nimcp_exception_system_init();
        nimcp_exception_clear_current();
    }

    void TearDown() override {
        nimcp_exception_clear_current();
        nimcp_exception_system_shutdown();
    }
};

//=============================================================================
// Memory Exception Type Regression Tests
// REGRESSION: nimcp_memory_exception_t structure and creation must be stable
//=============================================================================

TEST_F(TypedExceptionRegressionTest, MemoryExceptionCreation) {
    // REGRESSION: nimcp_memory_exception_create must set all fields correctly

    nimcp_memory_exception_t* mex = nimcp_memory_exception_create(
        NIMCP_ERROR_NO_MEMORY,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        1024,  // requested_size
        "Memory allocation failed for buffer of %zu bytes", (size_t)1024
    );

    ASSERT_NE(mex, nullptr) << "nimcp_memory_exception_create must return non-NULL";

    // Verify base fields
    EXPECT_EQ(mex->base.type, EXCEPTION_TYPE_MEMORY)
        << "Type must be EXCEPTION_TYPE_MEMORY";
    EXPECT_EQ(mex->base.code, NIMCP_ERROR_NO_MEMORY)
        << "Code must match input";
    EXPECT_EQ(mex->base.severity, EXCEPTION_SEVERITY_SEVERE)
        << "Severity must match input";
    EXPECT_EQ(mex->base.category, EXCEPTION_CATEGORY_MEMORY)
        << "Category must be EXCEPTION_CATEGORY_MEMORY";

    // Verify memory-specific fields
    EXPECT_EQ(mex->requested_size, 1024u)
        << "requested_size must be set correctly";

    // Verify message contains formatted text
    EXPECT_NE(strstr(mex->base.message, "1024"), nullptr)
        << "Message must contain formatted size";

    nimcp_exception_unref((nimcp_exception_t*)mex);
}

TEST_F(TypedExceptionRegressionTest, MemoryExceptionBaseCasting) {
    // REGRESSION: Memory exception must be castable to base type

    nimcp_memory_exception_t* mex = nimcp_memory_exception_create(
        NIMCP_ERROR_NO_MEMORY,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        2048,
        "Cast test"
    );

    ASSERT_NE(mex, nullptr);

    // Cast to base and verify
    nimcp_exception_t* base = (nimcp_exception_t*)mex;
    EXPECT_EQ(base->type, EXCEPTION_TYPE_MEMORY)
        << "Base cast must preserve type";
    EXPECT_EQ(base->code, NIMCP_ERROR_NO_MEMORY)
        << "Base cast must preserve code";

    // Cast back and verify memory-specific fields
    nimcp_memory_exception_t* mex_back = (nimcp_memory_exception_t*)base;
    EXPECT_EQ(mex_back->requested_size, 2048u)
        << "Cast back must preserve memory-specific fields";

    nimcp_exception_unref(base);
}

TEST_F(TypedExceptionRegressionTest, MemoryExceptionMacroIntegration) {
    // REGRESSION: NIMCP_THROW_MEMORY macro must create memory exception

    static nimcp_exception_t* captured_ex = nullptr;

    nimcp_handler_options_t opts;
    nimcp_handler_default_options(&opts);
    opts.name = "memory_macro_handler";
    opts.priority = NIMCP_HANDLER_PRIORITY_HIGH;
    opts.handler = [](nimcp_exception_t* ex, void* user_data) -> bool {
        captured_ex = nimcp_exception_ref(ex);
        return true;
    };

    nimcp_handler_registration_t* reg = nimcp_handler_register(&opts);
    ASSERT_NE(reg, nullptr);

    // Use macro
    NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, 4096, "Macro test: requested %zu", (size_t)4096);

    ASSERT_NE(captured_ex, nullptr);
    EXPECT_EQ(captured_ex->type, EXCEPTION_TYPE_MEMORY)
        << "NIMCP_THROW_MEMORY must create memory exception";
    EXPECT_TRUE(captured_ex->presented_to_immune)
        << "NIMCP_THROW_MEMORY must present to immune";

    nimcp_memory_exception_t* mex = (nimcp_memory_exception_t*)captured_ex;
    EXPECT_EQ(mex->requested_size, 4096u)
        << "NIMCP_THROW_MEMORY must set requested_size";

    nimcp_exception_unref(captured_ex);
    captured_ex = nullptr;
    nimcp_handler_unregister(reg);
}

//=============================================================================
// Brain Exception Type Regression Tests
// REGRESSION: nimcp_brain_exception_t structure and creation must be stable
//=============================================================================

TEST_F(TypedExceptionRegressionTest, BrainExceptionCreation) {
    // REGRESSION: nimcp_brain_exception_create must set all fields correctly

    nimcp_brain_exception_t* bex = nimcp_brain_exception_create(
        NIMCP_ERROR_BRAIN_CREATION,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        42,  // brain_id
        "visual_cortex",  // region_name
        "Brain region %s failed with ID %u", "visual_cortex", 42
    );

    ASSERT_NE(bex, nullptr) << "nimcp_brain_exception_create must return non-NULL";

    // Verify base fields
    EXPECT_EQ(bex->base.type, EXCEPTION_TYPE_BRAIN)
        << "Type must be EXCEPTION_TYPE_BRAIN";
    EXPECT_EQ(bex->base.code, NIMCP_ERROR_BRAIN_CREATION)
        << "Code must match input";
    EXPECT_EQ(bex->base.category, EXCEPTION_CATEGORY_BRAIN)
        << "Category must be EXCEPTION_CATEGORY_BRAIN";

    // Verify brain-specific fields
    EXPECT_EQ(bex->brain_id, 42u)
        << "brain_id must be set correctly";
    EXPECT_STREQ(bex->region_name, "visual_cortex")
        << "region_name must be set correctly";

    nimcp_exception_unref((nimcp_exception_t*)bex);
}

TEST_F(TypedExceptionRegressionTest, BrainExceptionMacroIntegration) {
    // REGRESSION: NIMCP_THROW_BRAIN macro must create brain exception

    static nimcp_exception_t* captured_ex = nullptr;

    nimcp_handler_options_t opts;
    nimcp_handler_default_options(&opts);
    opts.name = "brain_macro_handler";
    opts.priority = NIMCP_HANDLER_PRIORITY_HIGH;
    opts.handler = [](nimcp_exception_t* ex, void* user_data) -> bool {
        captured_ex = nimcp_exception_ref(ex);
        return true;
    };

    nimcp_handler_registration_t* reg = nimcp_handler_register(&opts);
    ASSERT_NE(reg, nullptr);

    // Use macro
    NIMCP_THROW_BRAIN(NIMCP_ERROR_NETWORK_CREATION, 100, "prefrontal", "Network error in %s", "prefrontal");

    ASSERT_NE(captured_ex, nullptr);
    EXPECT_EQ(captured_ex->type, EXCEPTION_TYPE_BRAIN)
        << "NIMCP_THROW_BRAIN must create brain exception";

    nimcp_brain_exception_t* bex = (nimcp_brain_exception_t*)captured_ex;
    EXPECT_EQ(bex->brain_id, 100u)
        << "NIMCP_THROW_BRAIN must set brain_id";
    EXPECT_STREQ(bex->region_name, "prefrontal")
        << "NIMCP_THROW_BRAIN must set region_name";

    nimcp_exception_unref(captured_ex);
    captured_ex = nullptr;
    nimcp_handler_unregister(reg);
}

//=============================================================================
// I/O Exception Type Regression Tests
// REGRESSION: nimcp_io_exception_t structure and creation must be stable
//=============================================================================

TEST_F(TypedExceptionRegressionTest, IOExceptionCreation) {
    // REGRESSION: nimcp_io_exception_create must set all fields correctly

    nimcp_io_exception_t* iex = nimcp_io_exception_create(
        NIMCP_ERROR_FILE_NOT_FOUND,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "/path/to/file.dat",  // path
        "File not found: %s", "/path/to/file.dat"
    );

    ASSERT_NE(iex, nullptr) << "nimcp_io_exception_create must return non-NULL";

    // Verify base fields
    EXPECT_EQ(iex->base.type, EXCEPTION_TYPE_IO)
        << "Type must be EXCEPTION_TYPE_IO";
    EXPECT_EQ(iex->base.code, NIMCP_ERROR_FILE_NOT_FOUND)
        << "Code must match input";
    EXPECT_EQ(iex->base.category, EXCEPTION_CATEGORY_IO)
        << "Category must be EXCEPTION_CATEGORY_IO";

    // Verify I/O-specific fields
    EXPECT_STREQ(iex->path, "/path/to/file.dat")
        << "path must be set correctly";

    nimcp_exception_unref((nimcp_exception_t*)iex);
}

TEST_F(TypedExceptionRegressionTest, IOExceptionMacroIntegration) {
    // REGRESSION: NIMCP_THROW_IO macro must create I/O exception

    static nimcp_exception_t* captured_ex = nullptr;

    nimcp_handler_options_t opts;
    nimcp_handler_default_options(&opts);
    opts.name = "io_macro_handler";
    opts.priority = NIMCP_HANDLER_PRIORITY_HIGH;
    opts.handler = [](nimcp_exception_t* ex, void* user_data) -> bool {
        captured_ex = nimcp_exception_ref(ex);
        return true;
    };

    nimcp_handler_registration_t* reg = nimcp_handler_register(&opts);
    ASSERT_NE(reg, nullptr);

    // Use macro
    NIMCP_THROW_IO(NIMCP_ERROR_FILE_READ, "/data/config.json", "Read error on %s", "/data/config.json");

    ASSERT_NE(captured_ex, nullptr);
    EXPECT_EQ(captured_ex->type, EXCEPTION_TYPE_IO)
        << "NIMCP_THROW_IO must create I/O exception";

    nimcp_io_exception_t* iex = (nimcp_io_exception_t*)captured_ex;
    EXPECT_STREQ(iex->path, "/data/config.json")
        << "NIMCP_THROW_IO must set path";

    nimcp_exception_unref(captured_ex);
    captured_ex = nullptr;
    nimcp_handler_unregister(reg);
}

//=============================================================================
// Threading Exception Type Regression Tests
// REGRESSION: nimcp_threading_exception_t structure and creation must be stable
//=============================================================================

TEST_F(TypedExceptionRegressionTest, ThreadingExceptionCreation) {
    // REGRESSION: nimcp_threading_exception_create must set all fields correctly

    nimcp_threading_exception_t* tex = nimcp_threading_exception_create(
        NIMCP_ERROR_DEADLOCK,
        EXCEPTION_SEVERITY_CRITICAL,
        __FILE__, __LINE__, __func__,
        12345,  // thread_id
        "Deadlock detected in thread %lu", (unsigned long)12345
    );

    ASSERT_NE(tex, nullptr) << "nimcp_threading_exception_create must return non-NULL";

    // Verify base fields
    EXPECT_EQ(tex->base.type, EXCEPTION_TYPE_THREADING)
        << "Type must be EXCEPTION_TYPE_THREADING";
    EXPECT_EQ(tex->base.code, NIMCP_ERROR_DEADLOCK)
        << "Code must match input";
    EXPECT_EQ(tex->base.category, EXCEPTION_CATEGORY_THREADING)
        << "Category must be EXCEPTION_CATEGORY_THREADING";

    // Verify threading-specific fields
    EXPECT_EQ(tex->thread_id, 12345u)
        << "thread_id must be set correctly";

    nimcp_exception_unref((nimcp_exception_t*)tex);
}

TEST_F(TypedExceptionRegressionTest, ThreadingExceptionMacroIntegration) {
    // REGRESSION: NIMCP_THROW_THREADING macro must create threading exception

    static nimcp_exception_t* captured_ex = nullptr;

    nimcp_handler_options_t opts;
    nimcp_handler_default_options(&opts);
    opts.name = "threading_macro_handler";
    opts.priority = NIMCP_HANDLER_PRIORITY_HIGH;
    opts.handler = [](nimcp_exception_t* ex, void* user_data) -> bool {
        captured_ex = nimcp_exception_ref(ex);
        return true;
    };

    nimcp_handler_registration_t* reg = nimcp_handler_register(&opts);
    ASSERT_NE(reg, nullptr);

    // Use macro
    NIMCP_THROW_THREADING(NIMCP_ERROR_MUTEX_LOCK, 67890, "Lock failed for thread %lu", (unsigned long)67890);

    ASSERT_NE(captured_ex, nullptr);
    EXPECT_EQ(captured_ex->type, EXCEPTION_TYPE_THREADING)
        << "NIMCP_THROW_THREADING must create threading exception";

    nimcp_threading_exception_t* tex = (nimcp_threading_exception_t*)captured_ex;
    EXPECT_EQ(tex->thread_id, 67890u)
        << "NIMCP_THROW_THREADING must set thread_id";

    nimcp_exception_unref(captured_ex);
    captured_ex = nullptr;
    nimcp_handler_unregister(reg);
}

//=============================================================================
// Security Exception Type Regression Tests
// REGRESSION: nimcp_security_exception_t structure and creation must be stable
//=============================================================================

TEST_F(TypedExceptionRegressionTest, SecurityExceptionCreation) {
    // REGRESSION: nimcp_security_exception_create must set all fields correctly

    nimcp_security_exception_t* sex = nimcp_security_exception_create(
        NIMCP_ERROR_SECURITY_THREAT,
        EXCEPTION_SEVERITY_CRITICAL,  // Security is always critical
        __FILE__, __LINE__, __func__,
        3,  // threat_type
        "Security threat type %u detected", 3
    );

    ASSERT_NE(sex, nullptr) << "nimcp_security_exception_create must return non-NULL";

    // Verify base fields
    EXPECT_EQ(sex->base.type, EXCEPTION_TYPE_SECURITY)
        << "Type must be EXCEPTION_TYPE_SECURITY";
    EXPECT_EQ(sex->base.code, NIMCP_ERROR_SECURITY_THREAT)
        << "Code must match input";

    // Verify security-specific fields
    EXPECT_EQ(sex->threat_type, 3u)
        << "threat_type must be set correctly";

    nimcp_exception_unref((nimcp_exception_t*)sex);
}

TEST_F(TypedExceptionRegressionTest, SecurityExceptionMacroIntegration) {
    // REGRESSION: NIMCP_THROW_SECURITY macro must create security exception with CRITICAL severity

    static nimcp_exception_t* captured_ex = nullptr;

    nimcp_handler_options_t opts;
    nimcp_handler_default_options(&opts);
    opts.name = "security_macro_handler";
    opts.priority = NIMCP_HANDLER_PRIORITY_HIGH;
    opts.handler = [](nimcp_exception_t* ex, void* user_data) -> bool {
        captured_ex = nimcp_exception_ref(ex);
        return true;
    };

    nimcp_handler_registration_t* reg = nimcp_handler_register(&opts);
    ASSERT_NE(reg, nullptr);

    // Use macro
    NIMCP_THROW_SECURITY(NIMCP_ERROR_BBB_REJECTED, 5, "BBB rejected threat type %u", 5);

    ASSERT_NE(captured_ex, nullptr);
    EXPECT_EQ(captured_ex->type, EXCEPTION_TYPE_SECURITY)
        << "NIMCP_THROW_SECURITY must create security exception";
    EXPECT_EQ(captured_ex->severity, EXCEPTION_SEVERITY_CRITICAL)
        << "NIMCP_THROW_SECURITY must always set CRITICAL severity";

    nimcp_security_exception_t* sex = (nimcp_security_exception_t*)captured_ex;
    EXPECT_EQ(sex->threat_type, 5u)
        << "NIMCP_THROW_SECURITY must set threat_type";

    nimcp_exception_unref(captured_ex);
    captured_ex = nullptr;
    nimcp_handler_unregister(reg);
}

//=============================================================================
// GPU Exception Type Regression Tests
// REGRESSION: nimcp_gpu_exception_t structure and creation must be stable
//=============================================================================

TEST_F(TypedExceptionRegressionTest, GPUExceptionCreation) {
    // REGRESSION: nimcp_gpu_exception_create must set all fields correctly

    nimcp_gpu_exception_t* gex = nimcp_gpu_exception_create(
        NIMCP_ERROR_GPU_MEMORY,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        0,  // device_id
        1,  // cuda_error
        "GPU %d CUDA error %d", 0, 1
    );

    ASSERT_NE(gex, nullptr) << "nimcp_gpu_exception_create must return non-NULL";

    // Verify base fields
    EXPECT_EQ(gex->base.type, EXCEPTION_TYPE_GPU)
        << "Type must be EXCEPTION_TYPE_GPU";
    EXPECT_EQ(gex->base.code, NIMCP_ERROR_GPU_MEMORY)
        << "Code must match input";

    // Verify GPU-specific fields
    EXPECT_EQ(gex->device_id, 0)
        << "device_id must be set correctly";
    EXPECT_EQ(gex->cuda_error, 1)
        << "cuda_error must be set correctly";

    nimcp_exception_unref((nimcp_exception_t*)gex);
}

TEST_F(TypedExceptionRegressionTest, GPUExceptionMacroIntegration) {
    // REGRESSION: NIMCP_THROW_GPU macro must create GPU exception

    static nimcp_exception_t* captured_ex = nullptr;

    nimcp_handler_options_t opts;
    nimcp_handler_default_options(&opts);
    opts.name = "gpu_macro_handler";
    opts.priority = NIMCP_HANDLER_PRIORITY_HIGH;
    opts.handler = [](nimcp_exception_t* ex, void* user_data) -> bool {
        captured_ex = nimcp_exception_ref(ex);
        return true;
    };

    nimcp_handler_registration_t* reg = nimcp_handler_register(&opts);
    ASSERT_NE(reg, nullptr);

    // Use macro
    NIMCP_THROW_GPU(NIMCP_ERROR_KERNEL_LAUNCH, 2, 700, "Kernel launch failed on device %d, CUDA %d", 2, 700);

    ASSERT_NE(captured_ex, nullptr);
    EXPECT_EQ(captured_ex->type, EXCEPTION_TYPE_GPU)
        << "NIMCP_THROW_GPU must create GPU exception";

    nimcp_gpu_exception_t* gex = (nimcp_gpu_exception_t*)captured_ex;
    EXPECT_EQ(gex->device_id, 2)
        << "NIMCP_THROW_GPU must set device_id";
    EXPECT_EQ(gex->cuda_error, 700)
        << "NIMCP_THROW_GPU must set cuda_error";

    nimcp_exception_unref(captured_ex);
    captured_ex = nullptr;
    nimcp_handler_unregister(reg);
}

//=============================================================================
// Signal Exception Type Regression Tests
// REGRESSION: nimcp_signal_exception_t structure and creation must be stable
//=============================================================================

TEST_F(TypedExceptionRegressionTest, SignalExceptionCreation) {
    // REGRESSION: nimcp_signal_exception_create must set all fields correctly

    nimcp_signal_exception_t* sex = nimcp_signal_exception_create(
        11,  // SIGSEGV
        (void*)0xDEADBEEF,  // fault_address
        __FILE__, __LINE__, __func__,
        "Segmentation fault at address %p", (void*)0xDEADBEEF
    );

    ASSERT_NE(sex, nullptr) << "nimcp_signal_exception_create must return non-NULL";

    // Verify base fields
    EXPECT_EQ(sex->base.type, EXCEPTION_TYPE_SIGNAL)
        << "Type must be EXCEPTION_TYPE_SIGNAL";
    EXPECT_EQ(sex->base.category, EXCEPTION_CATEGORY_SIGNAL)
        << "Category must be EXCEPTION_CATEGORY_SIGNAL";

    // Verify signal-specific fields
    EXPECT_EQ(sex->signal_number, 11)
        << "signal_number must be set correctly (SIGSEGV = 11)";
    EXPECT_EQ(sex->fault_address, (void*)0xDEADBEEF)
        << "fault_address must be set correctly";

    nimcp_exception_unref((nimcp_exception_t*)sex);
}

TEST_F(TypedExceptionRegressionTest, SignalToErrorCodeMapping) {
    // REGRESSION: nimcp_signal_to_error_code must map signals consistently

    EXPECT_EQ(nimcp_signal_to_error_code(11), NIMCP_ERROR_SIGSEGV)  // SIGSEGV
        << "SIGSEGV must map to NIMCP_ERROR_SIGSEGV";
    EXPECT_EQ(nimcp_signal_to_error_code(6), NIMCP_ERROR_SIGABRT)   // SIGABRT
        << "SIGABRT must map to NIMCP_ERROR_SIGABRT";
    EXPECT_EQ(nimcp_signal_to_error_code(8), NIMCP_ERROR_SIGFPE)    // SIGFPE
        << "SIGFPE must map to NIMCP_ERROR_SIGFPE";
    EXPECT_EQ(nimcp_signal_to_error_code(7), NIMCP_ERROR_SIGBUS)    // SIGBUS
        << "SIGBUS must map to NIMCP_ERROR_SIGBUS";
    EXPECT_EQ(nimcp_signal_to_error_code(4), NIMCP_ERROR_SIGILL)    // SIGILL
        << "SIGILL must map to NIMCP_ERROR_SIGILL";
}

TEST_F(TypedExceptionRegressionTest, SignalNameConversion) {
    // REGRESSION: nimcp_signal_name must return consistent strings

    EXPECT_STREQ(nimcp_signal_name(11), "SIGSEGV")
        << "Signal 11 must return 'SIGSEGV'";
    EXPECT_STREQ(nimcp_signal_name(6), "SIGABRT")
        << "Signal 6 must return 'SIGABRT'";
    EXPECT_STREQ(nimcp_signal_name(8), "SIGFPE")
        << "Signal 8 must return 'SIGFPE'";
    EXPECT_STREQ(nimcp_signal_name(7), "SIGBUS")
        << "Signal 7 must return 'SIGBUS'";
    EXPECT_STREQ(nimcp_signal_name(4), "SIGILL")
        << "Signal 4 must return 'SIGILL'";
}

//=============================================================================
// Aggregate Exception Type Regression Tests
// REGRESSION: nimcp_aggregate_exception_t structure and operations must be stable
//=============================================================================

TEST_F(TypedExceptionRegressionTest, AggregateExceptionCreation) {
    // REGRESSION: nimcp_aggregate_exception_create must return valid aggregate

    nimcp_aggregate_exception_t* agg = nimcp_aggregate_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Batch operation failed with multiple errors"
    );

    ASSERT_NE(agg, nullptr) << "nimcp_aggregate_exception_create must return non-NULL";

    // Verify base fields
    EXPECT_EQ(agg->base.type, EXCEPTION_TYPE_AGGREGATE)
        << "Type must be EXCEPTION_TYPE_AGGREGATE";
    EXPECT_EQ(agg->base.code, NIMCP_ERROR_OPERATION_FAILED)
        << "Code must match input";

    // Verify initial state
    EXPECT_EQ(nimcp_aggregate_exception_count(agg), 0u)
        << "Initial child count must be 0";

    nimcp_exception_unref((nimcp_exception_t*)agg);
}

TEST_F(TypedExceptionRegressionTest, AggregateExceptionAddChildren) {
    // REGRESSION: nimcp_aggregate_exception_add must add children correctly

    nimcp_aggregate_exception_t* agg = nimcp_aggregate_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Aggregate test"
    );

    ASSERT_NE(agg, nullptr);

    // Add first child
    nimcp_exception_t* child1 = nimcp_exception_create(
        NIMCP_ERROR_FILE_READ,
        EXCEPTION_SEVERITY_WARNING,
        __FILE__, __LINE__, __func__,
        "Child 1"
    );

    int result = nimcp_aggregate_exception_add(agg, child1);
    EXPECT_EQ(result, 0) << "add must return 0 on success";
    EXPECT_EQ(nimcp_aggregate_exception_count(agg), 1u)
        << "Count must be 1 after adding child";

    // Add second child
    nimcp_exception_t* child2 = nimcp_exception_create(
        NIMCP_ERROR_FILE_WRITE,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Child 2"
    );

    result = nimcp_aggregate_exception_add(agg, child2);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(nimcp_aggregate_exception_count(agg), 2u)
        << "Count must be 2 after adding second child";

    // Retrieve children
    nimcp_exception_t* retrieved1 = nimcp_aggregate_exception_get(agg, 0);
    EXPECT_EQ(retrieved1, child1) << "Index 0 must return first child";

    nimcp_exception_t* retrieved2 = nimcp_aggregate_exception_get(agg, 1);
    EXPECT_EQ(retrieved2, child2) << "Index 1 must return second child";

    // Invalid index returns NULL
    nimcp_exception_t* invalid = nimcp_aggregate_exception_get(agg, 100);
    EXPECT_EQ(invalid, nullptr) << "Invalid index must return NULL";

    nimcp_exception_unref((nimcp_exception_t*)agg);
}

TEST_F(TypedExceptionRegressionTest, AggregateExceptionMaxChildren) {
    // REGRESSION: Aggregate must respect NIMCP_EXCEPTION_MAX_CHILDREN limit

    nimcp_aggregate_exception_t* agg = nimcp_aggregate_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Max children test"
    );

    ASSERT_NE(agg, nullptr);

    // Add up to max children
    for (int i = 0; i < NIMCP_EXCEPTION_MAX_CHILDREN; i++) {
        nimcp_exception_t* child = nimcp_exception_create(
            NIMCP_ERROR_UNKNOWN,
            EXCEPTION_SEVERITY_INFO,
            __FILE__, __LINE__, __func__,
            "Child %d", i
        );

        int result = nimcp_aggregate_exception_add(agg, child);
        EXPECT_EQ(result, 0) << "Adding child " << i << " must succeed";
    }

    EXPECT_EQ(nimcp_aggregate_exception_count(agg), (size_t)NIMCP_EXCEPTION_MAX_CHILDREN)
        << "Count must equal max";

    // Adding one more should fail
    nimcp_exception_t* extra = nimcp_exception_create(
        NIMCP_ERROR_UNKNOWN,
        EXCEPTION_SEVERITY_INFO,
        __FILE__, __LINE__, __func__,
        "Extra child"
    );

    int result = nimcp_aggregate_exception_add(agg, extra);
    EXPECT_EQ(result, -1) << "Adding beyond max must return -1";

    // Clean up extra (wasn't added to aggregate)
    nimcp_exception_unref(extra);
    nimcp_exception_unref((nimcp_exception_t*)agg);
}

//=============================================================================
// C Polymorphism Verification Tests
// REGRESSION: All typed exceptions must support safe base casting
//=============================================================================

TEST_F(TypedExceptionRegressionTest, AllTypesPolymorphicCasting) {
    // REGRESSION: All exception types must be safely castable to base

    // Memory
    nimcp_memory_exception_t* mem = nimcp_memory_exception_create(
        NIMCP_ERROR_NO_MEMORY, EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__, 1024, "mem");
    EXPECT_EQ(((nimcp_exception_t*)mem)->type, EXCEPTION_TYPE_MEMORY);
    nimcp_exception_unref((nimcp_exception_t*)mem);

    // Brain
    nimcp_brain_exception_t* brain = nimcp_brain_exception_create(
        NIMCP_ERROR_BRAIN_CREATION, EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__, 1, "region", "brain");
    EXPECT_EQ(((nimcp_exception_t*)brain)->type, EXCEPTION_TYPE_BRAIN);
    nimcp_exception_unref((nimcp_exception_t*)brain);

    // I/O
    nimcp_io_exception_t* io = nimcp_io_exception_create(
        NIMCP_ERROR_FILE_NOT_FOUND, EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__, "/path", "io");
    EXPECT_EQ(((nimcp_exception_t*)io)->type, EXCEPTION_TYPE_IO);
    nimcp_exception_unref((nimcp_exception_t*)io);

    // Threading
    nimcp_threading_exception_t* thread = nimcp_threading_exception_create(
        NIMCP_ERROR_DEADLOCK, EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__, 123, "thread");
    EXPECT_EQ(((nimcp_exception_t*)thread)->type, EXCEPTION_TYPE_THREADING);
    nimcp_exception_unref((nimcp_exception_t*)thread);

    // Security
    nimcp_security_exception_t* sec = nimcp_security_exception_create(
        NIMCP_ERROR_SECURITY_THREAT, EXCEPTION_SEVERITY_CRITICAL,
        __FILE__, __LINE__, __func__, 1, "security");
    EXPECT_EQ(((nimcp_exception_t*)sec)->type, EXCEPTION_TYPE_SECURITY);
    nimcp_exception_unref((nimcp_exception_t*)sec);

    // GPU
    nimcp_gpu_exception_t* gpu = nimcp_gpu_exception_create(
        NIMCP_ERROR_GPU, EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__, 0, 1, "gpu");
    EXPECT_EQ(((nimcp_exception_t*)gpu)->type, EXCEPTION_TYPE_GPU);
    nimcp_exception_unref((nimcp_exception_t*)gpu);

    // Signal
    nimcp_signal_exception_t* sig = nimcp_signal_exception_create(
        11, nullptr, __FILE__, __LINE__, __func__, "signal");
    EXPECT_EQ(((nimcp_exception_t*)sig)->type, EXCEPTION_TYPE_SIGNAL);
    nimcp_exception_unref((nimcp_exception_t*)sig);

    // Aggregate
    nimcp_aggregate_exception_t* agg = nimcp_aggregate_exception_create(
        NIMCP_ERROR_UNKNOWN, EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__, "aggregate");
    EXPECT_EQ(((nimcp_exception_t*)agg)->type, EXCEPTION_TYPE_AGGREGATE);
    nimcp_exception_unref((nimcp_exception_t*)agg);
}

//=============================================================================
// Memory Layout Stability Tests
// REGRESSION: Base struct must be first member for polymorphism to work
//=============================================================================

TEST_F(TypedExceptionRegressionTest, MemoryLayoutBaseFirst) {
    // REGRESSION: Verify base struct is at offset 0 in all derived types
    // This is critical for C polymorphism via casting

    // Memory exception
    nimcp_memory_exception_t mem;
    EXPECT_EQ((void*)&mem, (void*)&mem.base)
        << "nimcp_memory_exception_t.base must be at offset 0";

    // Brain exception
    nimcp_brain_exception_t brain;
    EXPECT_EQ((void*)&brain, (void*)&brain.base)
        << "nimcp_brain_exception_t.base must be at offset 0";

    // I/O exception
    nimcp_io_exception_t io;
    EXPECT_EQ((void*)&io, (void*)&io.base)
        << "nimcp_io_exception_t.base must be at offset 0";

    // Threading exception
    nimcp_threading_exception_t thread;
    EXPECT_EQ((void*)&thread, (void*)&thread.base)
        << "nimcp_threading_exception_t.base must be at offset 0";

    // Security exception
    nimcp_security_exception_t sec;
    EXPECT_EQ((void*)&sec, (void*)&sec.base)
        << "nimcp_security_exception_t.base must be at offset 0";

    // GPU exception
    nimcp_gpu_exception_t gpu;
    EXPECT_EQ((void*)&gpu, (void*)&gpu.base)
        << "nimcp_gpu_exception_t.base must be at offset 0";

    // Signal exception
    nimcp_signal_exception_t sig;
    EXPECT_EQ((void*)&sig, (void*)&sig.base)
        << "nimcp_signal_exception_t.base must be at offset 0";

    // Aggregate exception
    nimcp_aggregate_exception_t agg;
    EXPECT_EQ((void*)&agg, (void*)&agg.base)
        << "nimcp_aggregate_exception_t.base must be at offset 0";
}

//=============================================================================
// Type Identification Stability Tests
// REGRESSION: Type identification must work correctly for dispatch
//=============================================================================

TEST_F(TypedExceptionRegressionTest, TypeIdentificationForHandler) {
    // REGRESSION: Handlers must be able to identify exception types

    static nimcp_exception_type_t captured_type = EXCEPTION_TYPE_BASE;
    static nimcp_exception_t* captured_ex = nullptr;

    nimcp_handler_options_t opts;
    nimcp_handler_default_options(&opts);
    opts.name = "type_id_handler";
    opts.priority = NIMCP_HANDLER_PRIORITY_HIGH;
    opts.type_filter = EXCEPTION_TYPE_MEMORY;  // Only handle memory exceptions
    opts.handler = [](nimcp_exception_t* ex, void* user_data) -> bool {
        captured_type = ex->type;
        captured_ex = nimcp_exception_ref(ex);
        return true;
    };

    nimcp_handler_registration_t* reg = nimcp_handler_register(&opts);
    ASSERT_NE(reg, nullptr);

    // Throw a memory exception - should be handled
    NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, 512, "Memory test");

    EXPECT_EQ(captured_type, EXCEPTION_TYPE_MEMORY)
        << "Handler with type filter must receive matching type";
    ASSERT_NE(captured_ex, nullptr);
    nimcp_exception_unref(captured_ex);
    captured_ex = nullptr;

    nimcp_handler_unregister(reg);
}

//=============================================================================
// Exception Context Key-Value Tests
// REGRESSION: Typed exceptions must support context API
//=============================================================================

TEST_F(TypedExceptionRegressionTest, TypedExceptionContextSupport) {
    // REGRESSION: Typed exceptions must support context key-value operations

    nimcp_memory_exception_t* mex = nimcp_memory_exception_create(
        NIMCP_ERROR_NO_MEMORY,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        2048,
        "Context test"
    );

    ASSERT_NE(mex, nullptr);

    // Add context via base pointer
    nimcp_exception_t* base = (nimcp_exception_t*)mex;

    int result = nimcp_exception_set_context(base, "pool_name", "main_arena");
    EXPECT_EQ(result, 0) << "set_context must work on typed exception";

    result = nimcp_exception_set_context(base, "fragmentation", "45%");
    EXPECT_EQ(result, 0);

    // Verify context
    const char* pool = nimcp_exception_get_context(base, "pool_name");
    EXPECT_STREQ(pool, "main_arena");

    const char* frag = nimcp_exception_get_context(base, "fragmentation");
    EXPECT_STREQ(frag, "45%");

    EXPECT_EQ(nimcp_exception_context_count(base), 2u);

    nimcp_exception_unref(base);
}

//=============================================================================
// Exception Cause Chain with Typed Exceptions
// REGRESSION: Cause chain must work with mixed exception types
//=============================================================================

TEST_F(TypedExceptionRegressionTest, MixedTypeCauseChain) {
    // REGRESSION: Cause chain must work across different exception types

    // Create root cause (I/O exception)
    nimcp_io_exception_t* io_cause = nimcp_io_exception_create(
        NIMCP_ERROR_FILE_READ,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "/data/weights.bin",
        "Failed to read weights file"
    );

    // Create wrapper (memory exception)
    nimcp_memory_exception_t* mem_ex = nimcp_memory_exception_create(
        NIMCP_ERROR_NO_MEMORY,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        1024*1024,
        "Cannot load weights into memory"
    );

    // Chain them
    nimcp_exception_ref((nimcp_exception_t*)io_cause);  // Add ref for set_cause
    nimcp_exception_set_cause((nimcp_exception_t*)mem_ex, (nimcp_exception_t*)io_cause);

    // Verify chain
    nimcp_exception_t* retrieved_cause = nimcp_exception_get_cause((nimcp_exception_t*)mem_ex);
    ASSERT_NE(retrieved_cause, nullptr);
    EXPECT_EQ(retrieved_cause->type, EXCEPTION_TYPE_IO)
        << "Cause must be I/O exception";
    EXPECT_EQ(retrieved_cause->code, NIMCP_ERROR_FILE_READ);

    // Clean up
    nimcp_exception_unref((nimcp_exception_t*)io_cause);
    nimcp_exception_unref((nimcp_exception_t*)mem_ex);
}

//=============================================================================
// Epitope Generation for Typed Exceptions
// REGRESSION: All typed exceptions must generate valid epitopes
//=============================================================================

TEST_F(TypedExceptionRegressionTest, TypedExceptionEpitopeGeneration) {
    // REGRESSION: nimcp_exception_generate_epitope must work for all types

    nimcp_memory_exception_t* mex = nimcp_memory_exception_create(
        NIMCP_ERROR_NO_MEMORY,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        1024,
        "Epitope test"
    );

    ASSERT_NE(mex, nullptr);

    // Generate epitope
    size_t len = nimcp_exception_generate_epitope((nimcp_exception_t*)mex);
    EXPECT_GT(len, 0u) << "Epitope length must be > 0";
    EXPECT_LE(len, NIMCP_EXCEPTION_EPITOPE_SIZE)
        << "Epitope length must not exceed max size";

    // Verify epitope is set in struct
    EXPECT_EQ(mex->base.epitope_len, len);

    nimcp_exception_unref((nimcp_exception_t*)mex);
}

//=============================================================================
// Suggested Recovery Action Tests
// REGRESSION: nimcp_exception_get_suggested_recovery must return appropriate actions
//=============================================================================

TEST_F(TypedExceptionRegressionTest, SuggestedRecoveryForTypedExceptions) {
    // REGRESSION: Suggested recovery must be appropriate for exception type

    // Memory exception should suggest GC
    nimcp_memory_exception_t* mex = nimcp_memory_exception_create(
        NIMCP_ERROR_NO_MEMORY,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        1024,
        "Recovery test"
    );
    nimcp_exception_recovery_action_t action = nimcp_exception_get_suggested_recovery((nimcp_exception_t*)mex);
    EXPECT_TRUE(action == EXCEPTION_RECOVERY_GC || action == EXCEPTION_RECOVERY_COMPACT)
        << "Memory exception should suggest GC or COMPACT";
    nimcp_exception_unref((nimcp_exception_t*)mex);

    // Threading exception should suggest thread restart
    nimcp_threading_exception_t* tex = nimcp_threading_exception_create(
        NIMCP_ERROR_DEADLOCK,
        EXCEPTION_SEVERITY_CRITICAL,
        __FILE__, __LINE__, __func__,
        123,
        "Deadlock test"
    );
    action = nimcp_exception_get_suggested_recovery((nimcp_exception_t*)tex);
    EXPECT_TRUE(action == EXCEPTION_RECOVERY_RESTART_THREAD || action == EXCEPTION_RECOVERY_RESTART_COMPONENT)
        << "Threading exception should suggest RESTART_THREAD or RESTART_COMPONENT";
    nimcp_exception_unref((nimcp_exception_t*)tex);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
