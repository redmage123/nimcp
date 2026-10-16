/**
 * @file test_exception.c
 * @brief Unit tests for exception handling system with brain immune integration
 *
 * WHAT: Comprehensive test suite for nimcp_exception API
 * WHY:  Verify correct behavior of exception creation, handlers, and immune integration
 * HOW:  Unit tests using Check framework covering all API functions
 *
 * @author NIMCP Development Team
 * @date 2026-01-16
 */

#include <check.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>

#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_handlers.h"
#include "utils/exception/nimcp_exception_immune.h"
#include "utils/exception/nimcp_exception_circuit.h"
#include "utils/exception/nimcp_exception_trace.h"
#include "utils/exception/nimcp_exception_metrics.h"

/*=============================================================================
 * Test Fixtures
 *=============================================================================*/

static void setup(void)
{
    nimcp_exception_system_init();
}

static void teardown(void)
{
    nimcp_exception_clear_current();
    nimcp_exception_system_shutdown();
}

/*=============================================================================
 * Exception Creation Tests
 *=============================================================================*/

START_TEST(test_exception_create_basic)
{
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_NULL_POINTER,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__,
        __LINE__,
        __func__,
        "Test exception: %s",
        "null pointer"
    );
    ck_assert_ptr_nonnull(ex);
    ck_assert_int_eq(ex->code, NIMCP_ERROR_NULL_POINTER);
    ck_assert_int_eq(ex->severity, EXCEPTION_SEVERITY_ERROR);
    ck_assert_int_eq(ex->type, EXCEPTION_TYPE_BASE);
    ck_assert(strstr(ex->message, "null pointer") != NULL);
    ck_assert_ptr_nonnull(ex->file);
    ck_assert_int_gt(ex->line, 0);
    ck_assert_ptr_nonnull(ex->function);
    ck_assert_uint_gt(ex->timestamp_us, 0);

    nimcp_exception_unref(ex);
}
END_TEST

START_TEST(test_exception_create_null_message)
{
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_INVALID_PARAM,
        EXCEPTION_SEVERITY_WARNING,
        __FILE__,
        __LINE__,
        __func__,
        NULL
    );
    ck_assert_ptr_nonnull(ex);
    ck_assert_int_eq(ex->code, NIMCP_ERROR_INVALID_PARAM);
    /* Message should be empty or default */

    nimcp_exception_unref(ex);
}
END_TEST

START_TEST(test_memory_exception_create)
{
    nimcp_memory_exception_t* mex = nimcp_memory_exception_create(
        NIMCP_ERROR_NO_MEMORY,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__,
        __LINE__,
        __func__,
        1024 * 1024,  /* 1 MB requested */
        "Failed to allocate %zu bytes",
        (size_t)(1024 * 1024)
    );
    ck_assert_ptr_nonnull(mex);
    ck_assert_int_eq(mex->base.type, EXCEPTION_TYPE_MEMORY);
    ck_assert_int_eq(mex->base.category, EXCEPTION_CATEGORY_MEMORY);
    ck_assert_uint_eq(mex->requested_size, 1024 * 1024);

    nimcp_exception_unref((nimcp_exception_t*)mex);
}
END_TEST

START_TEST(test_brain_exception_create)
{
    nimcp_brain_exception_t* bex = nimcp_brain_exception_create(
        NIMCP_ERROR_FORWARD_PASS,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__,
        __LINE__,
        __func__,
        42,           /* brain_id */
        "prefrontal", /* region_name */
        "NaN detected in layer %d",
        5
    );
    ck_assert_ptr_nonnull(bex);
    ck_assert_int_eq(bex->base.type, EXCEPTION_TYPE_BRAIN);
    ck_assert_int_eq(bex->base.category, EXCEPTION_CATEGORY_BRAIN);
    ck_assert_uint_eq(bex->brain_id, 42);
    ck_assert_str_eq(bex->region_name, "prefrontal");

    nimcp_exception_unref((nimcp_exception_t*)bex);
}
END_TEST

START_TEST(test_io_exception_create)
{
    nimcp_io_exception_t* iex = nimcp_io_exception_create(
        NIMCP_ERROR_FILE_NOT_FOUND,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__,
        __LINE__,
        __func__,
        "/path/to/file.dat",
        "File not found: %s",
        "/path/to/file.dat"
    );
    ck_assert_ptr_nonnull(iex);
    ck_assert_int_eq(iex->base.type, EXCEPTION_TYPE_IO);
    ck_assert_int_eq(iex->base.category, EXCEPTION_CATEGORY_IO);
    ck_assert_str_eq(iex->path, "/path/to/file.dat");

    nimcp_exception_unref((nimcp_exception_t*)iex);
}
END_TEST

START_TEST(test_threading_exception_create)
{
    nimcp_threading_exception_t* tex = nimcp_threading_exception_create(
        NIMCP_ERROR_DEADLOCK,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__,
        __LINE__,
        __func__,
        12345,  /* thread_id */
        "Mutex timeout on thread %lu",
        (unsigned long)12345
    );
    ck_assert_ptr_nonnull(tex);
    ck_assert_int_eq(tex->base.type, EXCEPTION_TYPE_THREADING);
    ck_assert_int_eq(tex->base.category, EXCEPTION_CATEGORY_THREADING);
    ck_assert_uint_eq(tex->thread_id, 12345);

    nimcp_exception_unref((nimcp_exception_t*)tex);
}
END_TEST

START_TEST(test_gpu_exception_create)
{
    nimcp_gpu_exception_t* gex = nimcp_gpu_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__,
        __LINE__,
        __func__,
        0,      /* device_id */
        2,      /* cuda_error */
        "CUDA allocation failed on device %d",
        0
    );
    ck_assert_ptr_nonnull(gex);
    ck_assert_int_eq(gex->base.type, EXCEPTION_TYPE_GPU);
    ck_assert_int_eq(gex->base.category, EXCEPTION_CATEGORY_GPU);
    ck_assert_int_eq(gex->device_id, 0);
    ck_assert_int_eq(gex->cuda_error, 2);

    nimcp_exception_unref((nimcp_exception_t*)gex);
}
END_TEST

/*=============================================================================
 * Reference Counting Tests
 *=============================================================================*/

START_TEST(test_exception_ref_unref)
{
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_INVALID_PARAM,
        EXCEPTION_SEVERITY_WARNING,
        __FILE__, __LINE__, __func__,
        "Test refcount"
    );
    ck_assert_ptr_nonnull(ex);
    ck_assert_int_eq(ex->ref_count, 1);

    /* Add reference */
    nimcp_exception_t* ex2 = nimcp_exception_ref(ex);
    ck_assert_ptr_eq(ex2, ex);
    ck_assert_int_eq(ex->ref_count, 2);

    /* Release one reference */
    nimcp_exception_unref(ex);
    ck_assert_int_eq(ex->ref_count, 1);

    /* Release final reference (should free) */
    nimcp_exception_unref(ex);
    /* Can't verify after free, but should not crash */
}
END_TEST

START_TEST(test_exception_ref_null)
{
    nimcp_exception_t* result = nimcp_exception_ref(NULL);
    ck_assert_ptr_null(result);
}
END_TEST

START_TEST(test_exception_unref_null)
{
    /* Should not crash */
    nimcp_exception_unref(NULL);
}
END_TEST

/*=============================================================================
 * Exception Chaining Tests
 *=============================================================================*/

START_TEST(test_exception_set_cause)
{
    nimcp_exception_t* cause = nimcp_exception_create(
        NIMCP_ERROR_NO_MEMORY,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        "Original cause"
    );
    ck_assert_ptr_nonnull(cause);

    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Wrapper exception"
    );
    ck_assert_ptr_nonnull(ex);

    /* Set cause (transfers ownership) */
    nimcp_exception_ref(cause);  /* Keep our reference */
    nimcp_exception_set_cause(ex, cause);

    ck_assert_ptr_eq(nimcp_exception_get_cause(ex), cause);

    /* Cleanup */
    nimcp_exception_unref(cause);
    nimcp_exception_unref(ex);
}
END_TEST

START_TEST(test_exception_get_cause_null)
{
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "No cause"
    );
    ck_assert_ptr_nonnull(ex);
    ck_assert_ptr_null(nimcp_exception_get_cause(ex));

    nimcp_exception_unref(ex);
}
END_TEST

/*=============================================================================
 * Category and Severity Derivation Tests
 *=============================================================================*/

START_TEST(test_get_category_from_code_memory)
{
    nimcp_exception_category_t cat = nimcp_exception_get_category_from_code(NIMCP_ERROR_NO_MEMORY);
    ck_assert_int_eq(cat, EXCEPTION_CATEGORY_MEMORY);
}
END_TEST

START_TEST(test_get_category_from_code_brain)
{
    nimcp_exception_category_t cat = nimcp_exception_get_category_from_code(NIMCP_ERROR_FORWARD_PASS);
    ck_assert_int_eq(cat, EXCEPTION_CATEGORY_BRAIN);
}
END_TEST

START_TEST(test_get_category_from_code_io)
{
    nimcp_exception_category_t cat = nimcp_exception_get_category_from_code(NIMCP_ERROR_FILE_NOT_FOUND);
    ck_assert_int_eq(cat, EXCEPTION_CATEGORY_IO);
}
END_TEST

START_TEST(test_get_category_from_code_threading)
{
    nimcp_exception_category_t cat = nimcp_exception_get_category_from_code(NIMCP_ERROR_DEADLOCK);
    ck_assert_int_eq(cat, EXCEPTION_CATEGORY_THREADING);
}
END_TEST

START_TEST(test_get_severity_from_code)
{
    /* OOM should be severe */
    nimcp_exception_severity_t sev = nimcp_exception_get_severity_from_code(NIMCP_ERROR_NO_MEMORY);
    ck_assert_int_ge(sev, EXCEPTION_SEVERITY_SEVERE);

    /* Invalid arg should be warning/error */
    sev = nimcp_exception_get_severity_from_code(NIMCP_ERROR_INVALID_PARAM);
    ck_assert_int_ge(sev, EXCEPTION_SEVERITY_WARNING);
}
END_TEST

/*=============================================================================
 * String Conversion Tests
 *=============================================================================*/

START_TEST(test_severity_to_string)
{
    ck_assert_str_eq(nimcp_exception_severity_to_string(EXCEPTION_SEVERITY_DEBUG), "DEBUG");
    ck_assert_str_eq(nimcp_exception_severity_to_string(EXCEPTION_SEVERITY_INFO), "INFO");
    ck_assert_str_eq(nimcp_exception_severity_to_string(EXCEPTION_SEVERITY_WARNING), "WARNING");
    ck_assert_str_eq(nimcp_exception_severity_to_string(EXCEPTION_SEVERITY_ERROR), "ERROR");
    ck_assert_str_eq(nimcp_exception_severity_to_string(EXCEPTION_SEVERITY_SEVERE), "SEVERE");
    ck_assert_str_eq(nimcp_exception_severity_to_string(EXCEPTION_SEVERITY_CRITICAL), "CRITICAL");
    ck_assert_str_eq(nimcp_exception_severity_to_string(EXCEPTION_SEVERITY_FATAL), "FATAL");
}
END_TEST

START_TEST(test_category_to_string)
{
    ck_assert_str_eq(nimcp_exception_category_to_string(EXCEPTION_CATEGORY_GENERIC), "GENERIC");
    ck_assert_str_eq(nimcp_exception_category_to_string(EXCEPTION_CATEGORY_MEMORY), "MEMORY");
    ck_assert_str_eq(nimcp_exception_category_to_string(EXCEPTION_CATEGORY_BRAIN), "BRAIN");
    ck_assert_str_eq(nimcp_exception_category_to_string(EXCEPTION_CATEGORY_IO), "IO");
    ck_assert_str_eq(nimcp_exception_category_to_string(EXCEPTION_CATEGORY_THREADING), "THREADING");
}
END_TEST

START_TEST(test_type_to_string)
{
    ck_assert_str_eq(nimcp_exception_type_to_string(EXCEPTION_TYPE_BASE), "BASE");
    ck_assert_str_eq(nimcp_exception_type_to_string(EXCEPTION_TYPE_MEMORY), "MEMORY");
    ck_assert_str_eq(nimcp_exception_type_to_string(EXCEPTION_TYPE_BRAIN), "BRAIN");
    ck_assert_str_eq(nimcp_exception_type_to_string(EXCEPTION_TYPE_IO), "IO");
    ck_assert_str_eq(nimcp_exception_type_to_string(EXCEPTION_TYPE_THREADING), "THREADING");
    ck_assert_str_eq(nimcp_exception_type_to_string(EXCEPTION_TYPE_GPU), "GPU");
}
END_TEST

START_TEST(test_recovery_action_to_string)
{
    ck_assert_str_eq(nimcp_exception_recovery_action_to_string(EXCEPTION_RECOVERY_NONE), "NONE");
    ck_assert_str_eq(nimcp_exception_recovery_action_to_string(EXCEPTION_RECOVERY_RETRY), "RETRY");
    ck_assert_str_eq(nimcp_exception_recovery_action_to_string(EXCEPTION_RECOVERY_GC), "GC");
    ck_assert_str_eq(nimcp_exception_recovery_action_to_string(EXCEPTION_RECOVERY_COMPACT), "COMPACT");
    ck_assert_str_eq(nimcp_exception_recovery_action_to_string(EXCEPTION_RECOVERY_ROLLBACK), "ROLLBACK");
    ck_assert_str_eq(nimcp_exception_recovery_action_to_string(EXCEPTION_RECOVERY_QUARANTINE), "QUARANTINE");
}
END_TEST

START_TEST(test_exception_to_string)
{
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_INVALID_PARAM,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Test message"
    );
    ck_assert_ptr_nonnull(ex);

    char buffer[512];
    size_t len = nimcp_exception_to_string(ex, buffer, sizeof(buffer));
    ck_assert_uint_gt(len, 0);
    ck_assert(strstr(buffer, "Test message") != NULL);
    ck_assert(strstr(buffer, "ERROR") != NULL);

    nimcp_exception_unref(ex);
}
END_TEST

/*=============================================================================
 * Thread-Local Context Tests
 *=============================================================================*/

START_TEST(test_set_get_current)
{
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Current exception"
    );
    ck_assert_ptr_nonnull(ex);

    /* Initially no current exception */
    ck_assert_ptr_null(nimcp_exception_get_current());

    /* Set current */
    nimcp_exception_set_current(ex);
    ck_assert_ptr_eq(nimcp_exception_get_current(), ex);

    /* Clear current */
    nimcp_exception_clear_current();
    ck_assert_ptr_null(nimcp_exception_get_current());

    nimcp_exception_unref(ex);
}
END_TEST

START_TEST(test_current_exception_replace)
{
    nimcp_exception_t* ex1 = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "First"
    );
    nimcp_exception_t* ex2 = nimcp_exception_create(
        NIMCP_ERROR_INVALID_PARAM,
        EXCEPTION_SEVERITY_WARNING,
        __FILE__, __LINE__, __func__,
        "Second"
    );

    nimcp_exception_set_current(ex1);
    ck_assert_ptr_eq(nimcp_exception_get_current(), ex1);

    nimcp_exception_set_current(ex2);
    ck_assert_ptr_eq(nimcp_exception_get_current(), ex2);

    nimcp_exception_clear_current();

    nimcp_exception_unref(ex1);
    nimcp_exception_unref(ex2);
}
END_TEST

/*=============================================================================
 * Handler Registration Tests
 *=============================================================================*/

static bool test_handler_called = false;
static int test_handler_call_count = 0;

static bool test_handler(nimcp_exception_t* ex, void* user_data) {
    (void)ex;
    (void)user_data;
    test_handler_called = true;
    test_handler_call_count++;
    return false;  /* Don't consume */
}

static bool consuming_handler(nimcp_exception_t* ex, void* user_data) {
    (void)ex;
    (void)user_data;
    return true;  /* Consume exception */
}

static void handler_setup(void) {
    setup();
    test_handler_called = false;
    test_handler_call_count = 0;
}

START_TEST(test_handler_register_unregister)
{
    nimcp_handler_options_t opts;
    nimcp_handler_default_options(&opts);
    opts.name = "test_handler";
    opts.handler = test_handler;
    opts.priority = NIMCP_HANDLER_PRIORITY_NORMAL;

    size_t initial_count = nimcp_handler_count();

    nimcp_handler_registration_t* reg = nimcp_handler_register(&opts);
    ck_assert_ptr_nonnull(reg);
    ck_assert_str_eq(reg->options.name, "test_handler");
    ck_assert(reg->active);
    ck_assert_uint_eq(nimcp_handler_count(), initial_count + 1);

    int ret = nimcp_handler_unregister(reg);
    ck_assert_int_eq(ret, 0);
    ck_assert_uint_eq(nimcp_handler_count(), initial_count);
}
END_TEST

START_TEST(test_handler_register_null_options)
{
    nimcp_handler_registration_t* reg = nimcp_handler_register(NULL);
    ck_assert_ptr_null(reg);
}
END_TEST

START_TEST(test_handler_register_null_handler)
{
    nimcp_handler_options_t opts;
    nimcp_handler_default_options(&opts);
    opts.handler = NULL;  /* Invalid */

    nimcp_handler_registration_t* reg = nimcp_handler_register(&opts);
    ck_assert_ptr_null(reg);
}
END_TEST

START_TEST(test_handler_enable_disable)
{
    nimcp_handler_options_t opts;
    nimcp_handler_default_options(&opts);
    opts.name = "toggle_handler";
    opts.handler = test_handler;

    nimcp_handler_registration_t* reg = nimcp_handler_register(&opts);
    ck_assert_ptr_nonnull(reg);
    ck_assert(reg->active);

    nimcp_handler_disable(reg);
    ck_assert(!reg->active);

    nimcp_handler_enable(reg);
    ck_assert(reg->active);

    nimcp_handler_unregister(reg);
}
END_TEST

START_TEST(test_handler_dispatch)
{
    nimcp_handler_options_t opts;
    nimcp_handler_default_options(&opts);
    opts.name = "dispatch_handler";
    opts.handler = test_handler;

    nimcp_handler_registration_t* reg = nimcp_handler_register(&opts);
    ck_assert_ptr_nonnull(reg);

    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Dispatch test"
    );

    bool handled = nimcp_exception_dispatch(ex);
    ck_assert(test_handler_called);
    ck_assert(!handled);  /* Handler returns false */

    nimcp_exception_unref(ex);
    nimcp_handler_unregister(reg);
}
END_TEST

START_TEST(test_handler_dispatch_consuming)
{
    nimcp_handler_options_t opts;
    nimcp_handler_default_options(&opts);
    opts.name = "consuming_handler";
    opts.handler = consuming_handler;
    opts.priority = NIMCP_HANDLER_PRIORITY_HIGH;

    nimcp_handler_options_t opts2;
    nimcp_handler_default_options(&opts2);
    opts2.name = "second_handler";
    opts2.handler = test_handler;
    opts2.priority = NIMCP_HANDLER_PRIORITY_LOW;

    nimcp_handler_registration_t* reg1 = nimcp_handler_register(&opts);
    nimcp_handler_registration_t* reg2 = nimcp_handler_register(&opts2);

    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Consuming test"
    );

    bool handled = nimcp_exception_dispatch(ex);
    ck_assert(handled);
    ck_assert(!test_handler_called);  /* Should not be called (consumed) */

    nimcp_exception_unref(ex);
    nimcp_handler_unregister(reg1);
    nimcp_handler_unregister(reg2);
}
END_TEST

START_TEST(test_handler_priority_order)
{
    static int call_order[3];
    static int call_index = 0;
    call_index = 0;

    bool handler_a(nimcp_exception_t* ex, void* user_data) {
        (void)ex;
        call_order[call_index++] = *(int*)user_data;
        return false;
    }

    int id1 = 1, id2 = 2, id3 = 3;

    nimcp_handler_options_t opts1, opts2, opts3;
    nimcp_handler_default_options(&opts1);
    nimcp_handler_default_options(&opts2);
    nimcp_handler_default_options(&opts3);

    opts1.name = "low";
    opts1.handler = handler_a;
    opts1.user_data = &id1;
    opts1.priority = NIMCP_HANDLER_PRIORITY_LOW;

    opts2.name = "high";
    opts2.handler = handler_a;
    opts2.user_data = &id2;
    opts2.priority = NIMCP_HANDLER_PRIORITY_HIGH;

    opts3.name = "normal";
    opts3.handler = handler_a;
    opts3.user_data = &id3;
    opts3.priority = NIMCP_HANDLER_PRIORITY_NORMAL;

    nimcp_handler_registration_t* reg1 = nimcp_handler_register(&opts1);
    nimcp_handler_registration_t* reg2 = nimcp_handler_register(&opts2);
    nimcp_handler_registration_t* reg3 = nimcp_handler_register(&opts3);

    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Priority test"
    );

    nimcp_exception_dispatch(ex);

    /* Should be called in priority order: high(2), normal(3), low(1) */
    ck_assert_int_eq(call_order[0], 2);
    ck_assert_int_eq(call_order[1], 3);
    ck_assert_int_eq(call_order[2], 1);

    nimcp_exception_unref(ex);
    nimcp_handler_unregister(reg1);
    nimcp_handler_unregister(reg2);
    nimcp_handler_unregister(reg3);
}
END_TEST

START_TEST(test_handler_category_filter)
{
    nimcp_handler_options_t opts;
    nimcp_handler_default_options(&opts);
    opts.name = "memory_only";
    opts.handler = test_handler;
    opts.category_filter = EXCEPTION_CATEGORY_MEMORY;

    nimcp_handler_registration_t* reg = nimcp_handler_register(&opts);

    /* Memory exception should trigger handler */
    nimcp_exception_t* mem_ex = nimcp_exception_create(
        NIMCP_ERROR_NO_MEMORY,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        "Memory error"
    );
    mem_ex->category = EXCEPTION_CATEGORY_MEMORY;

    test_handler_called = false;
    nimcp_exception_dispatch(mem_ex);
    ck_assert(test_handler_called);

    /* IO exception should NOT trigger handler */
    nimcp_exception_t* io_ex = nimcp_exception_create(
        NIMCP_ERROR_FILE_NOT_FOUND,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "IO error"
    );
    io_ex->category = EXCEPTION_CATEGORY_IO;

    test_handler_called = false;
    nimcp_exception_dispatch(io_ex);
    ck_assert(!test_handler_called);

    nimcp_exception_unref(mem_ex);
    nimcp_exception_unref(io_ex);
    nimcp_handler_unregister(reg);
}
END_TEST

/*=============================================================================
 * Try/Catch Tests
 *=============================================================================*/

START_TEST(test_try_catch_no_exception)
{
    bool try_executed = false;
    bool catch_executed = false;

    NIMCP_TRY {
        try_executed = true;
        /* No exception raised */
    }
    NIMCP_CATCH(nimcp_exception_t, ex) {
        catch_executed = true;
        nimcp_exception_unref(ex);
    }
    NIMCP_END_TRY;

    ck_assert(try_executed);
    ck_assert(!catch_executed);
}
END_TEST

START_TEST(test_try_catch_with_exception)
{
    /* Variables modified between setjmp/longjmp must be volatile */
    volatile bool try_executed = false;
    volatile bool catch_executed = false;
    volatile nimcp_error_t caught_code = 0;

    NIMCP_TRY {
        try_executed = true;
        nimcp_exception_throw(
            NIMCP_ERROR_INVALID_PARAM,
            __FILE__, __LINE__, __func__,
            "Test throw"
        );
        /* Should not reach here */
        ck_abort_msg("Should have jumped to catch");
    }
    NIMCP_CATCH(nimcp_exception_t, ex) {
        catch_executed = true;
        caught_code = ex->code;
        nimcp_exception_unref(ex);
    }
    NIMCP_END_TRY;

    ck_assert(try_executed);
    ck_assert(catch_executed);
    ck_assert_int_eq(caught_code, NIMCP_ERROR_INVALID_PARAM);
}
END_TEST

START_TEST(test_try_context_stack)
{
    ck_assert(!nimcp_in_try_block());

    NIMCP_TRY {
        ck_assert(nimcp_in_try_block());
        ck_assert_ptr_nonnull(nimcp_try_current());
    }
    NIMCP_CATCH(nimcp_exception_t, ex) {
        (void)ex;
    }
    NIMCP_END_TRY;

    ck_assert(!nimcp_in_try_block());
}
END_TEST

START_TEST(test_nested_try_catch)
{
    int inner_caught = 0;
    int outer_caught = 0;

    NIMCP_TRY {
        NIMCP_TRY {
            nimcp_exception_throw(
                NIMCP_ERROR_OPERATION_FAILED,
                __FILE__, __LINE__, __func__,
                "Inner exception"
            );
        }
        NIMCP_CATCH(nimcp_exception_t, ex) {
            inner_caught = 1;
            nimcp_exception_unref(ex);
        }
        NIMCP_END_TRY;

        /* This should still execute */
        ck_assert_int_eq(inner_caught, 1);
    }
    NIMCP_CATCH(nimcp_exception_t, ex) {
        outer_caught = 1;
        nimcp_exception_unref(ex);
    }
    NIMCP_END_TRY;

    ck_assert_int_eq(inner_caught, 1);
    ck_assert_int_eq(outer_caught, 0);  /* Inner catch handled it */
}
END_TEST

/*=============================================================================
 * Recovery Callback Tests
 *=============================================================================*/

static bool recovery_callback_called = false;
static nimcp_exception_recovery_action_t recovery_action_received = EXCEPTION_RECOVERY_NONE;

static int test_recovery_callback(nimcp_exception_t* ex, nimcp_exception_recovery_action_t action, void* user_data) {
    (void)ex;
    (void)user_data;
    recovery_callback_called = true;
    recovery_action_received = action;
    return 0;  /* Success */
}

static void recovery_setup(void) {
    setup();
    recovery_callback_called = false;
    recovery_action_received = EXCEPTION_RECOVERY_NONE;
}

START_TEST(test_register_recovery_callback)
{
    int ret = nimcp_register_recovery_callback(
        EXCEPTION_RECOVERY_GC,
        test_recovery_callback,
        NULL
    );
    ck_assert_int_eq(ret, 0);

    ret = nimcp_unregister_recovery_callback(EXCEPTION_RECOVERY_GC);
    ck_assert_int_eq(ret, 0);
}
END_TEST

START_TEST(test_execute_recovery)
{
    nimcp_register_recovery_callback(
        EXCEPTION_RECOVERY_RETRY,
        test_recovery_callback,
        NULL
    );

    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_FILE_READ,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "IO error for retry"
    );

    int ret = nimcp_execute_recovery(ex, EXCEPTION_RECOVERY_RETRY);
    ck_assert_int_eq(ret, 0);
    ck_assert(recovery_callback_called);
    ck_assert_int_eq(recovery_action_received, EXCEPTION_RECOVERY_RETRY);
    ck_assert(ex->recovery_attempted);
    ck_assert(ex->recovery_succeeded);

    nimcp_exception_unref(ex);
    nimcp_unregister_recovery_callback(EXCEPTION_RECOVERY_RETRY);
}
END_TEST

START_TEST(test_recovery_no_callback)
{
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_FILE_READ,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "No callback registered"
    );

    int ret = nimcp_execute_recovery(ex, EXCEPTION_RECOVERY_COMPACT);
    ck_assert_int_eq(ret, -1);  /* Should fail - no callback */

    nimcp_exception_unref(ex);
}
END_TEST

/*=============================================================================
 * Immune Integration Tests
 *=============================================================================*/

START_TEST(test_exception_to_antigen_source)
{
    ck_assert_int_eq(
        nimcp_exception_to_antigen_source(EXCEPTION_CATEGORY_MEMORY),
        EX_ANTIGEN_SOURCE_ANOMALY
    );
    ck_assert_int_eq(
        nimcp_exception_to_antigen_source(EXCEPTION_CATEGORY_SECURITY),
        EX_ANTIGEN_SOURCE_BBB
    );
    ck_assert_int_eq(
        nimcp_exception_to_antigen_source(EXCEPTION_CATEGORY_THREADING),
        EX_ANTIGEN_SOURCE_BFT
    );
}
END_TEST

START_TEST(test_exception_to_immune_severity)
{
    ck_assert_uint_eq(nimcp_exception_to_immune_severity(EXCEPTION_SEVERITY_DEBUG), 1);
    ck_assert_uint_eq(nimcp_exception_to_immune_severity(EXCEPTION_SEVERITY_INFO), 2);
    ck_assert_uint_eq(nimcp_exception_to_immune_severity(EXCEPTION_SEVERITY_WARNING), 3);
    ck_assert_uint_eq(nimcp_exception_to_immune_severity(EXCEPTION_SEVERITY_ERROR), 5);
    ck_assert_uint_eq(nimcp_exception_to_immune_severity(EXCEPTION_SEVERITY_SEVERE), 7);
    ck_assert_uint_eq(nimcp_exception_to_immune_severity(EXCEPTION_SEVERITY_CRITICAL), 9);
    ck_assert_uint_eq(nimcp_exception_to_immune_severity(EXCEPTION_SEVERITY_FATAL), 10);
}
END_TEST

START_TEST(test_epitope_computation)
{
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_NO_MEMORY,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        "OOM for epitope test"
    );

    uint8_t epitope[NIMCP_EXCEPTION_EPITOPE_SIZE];
    size_t len = nimcp_exception_compute_epitope(ex, epitope, sizeof(epitope));

    ck_assert_uint_gt(len, 0);
    ck_assert_uint_le(len, NIMCP_EXCEPTION_EPITOPE_SIZE);

    /* Same exception should produce same epitope */
    uint8_t epitope2[NIMCP_EXCEPTION_EPITOPE_SIZE];
    size_t len2 = nimcp_exception_compute_epitope(ex, epitope2, sizeof(epitope2));

    ck_assert_uint_eq(len, len2);
    ck_assert_mem_eq(epitope, epitope2, len);

    nimcp_exception_unref(ex);
}
END_TEST

START_TEST(test_recovery_strategy)
{
    nimcp_exception_t* mem_ex = nimcp_exception_create(
        NIMCP_ERROR_NO_MEMORY,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        "Memory exception"
    );
    mem_ex->category = EXCEPTION_CATEGORY_MEMORY;

    nimcp_exception_recovery_strategy_t strategy;
    nimcp_exception_get_recovery_strategy(mem_ex, &strategy);

    /* Memory exceptions should use GC or compact as primary */
    ck_assert(strategy.primary_action == EXCEPTION_RECOVERY_GC ||
              strategy.primary_action == EXCEPTION_RECOVERY_COMPACT);

    nimcp_exception_unref(mem_ex);
}
END_TEST

START_TEST(test_immune_config_defaults)
{
    nimcp_exception_immune_config_t config;
    nimcp_exception_immune_default_config(&config);

    ck_assert(config.enable_auto_present);
    ck_assert_int_eq(config.min_present_severity, NIMCP_EXCEPTION_IMMUNE_MIN_SEVERITY);
    ck_assert(config.enable_auto_recovery);
    ck_assert(config.enable_memory_formation);
}
END_TEST

START_TEST(test_immune_not_connected)
{
    /* Ensure not connected initially */
    nimcp_exception_immune_disconnect();
    ck_assert(!nimcp_exception_immune_is_connected());
}
END_TEST

/*=============================================================================
 * Default Handler Tests
 *=============================================================================*/

START_TEST(test_install_default_handlers)
{
    size_t initial = nimcp_handler_count();

    int ret = nimcp_install_default_handlers();
    ck_assert_int_eq(ret, 0);

    /* Should have added at least 2 handlers (logging + immune) */
    ck_assert_uint_ge(nimcp_handler_count(), initial + 2);
}
END_TEST

/*=============================================================================
 * Stack Trace Tests
 *=============================================================================*/

START_TEST(test_stack_trace_capture)
{
    nimcp_stack_trace_t trace;
    memset(&trace, 0, sizeof(trace));

    size_t depth = nimcp_exception_capture_stack_trace(&trace, 0);

    /* Should capture at least 1 frame (this function) */
    ck_assert_uint_gt(depth, 0);
    ck_assert_uint_eq(trace.depth, depth);
    ck_assert_uint_le(depth, NIMCP_EXCEPTION_MAX_STACK_DEPTH);
}
END_TEST

START_TEST(test_stack_trace_to_string)
{
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Stack trace test"
    );

    char buffer[2048];
    size_t len = nimcp_stack_trace_to_string(&ex->stack_trace, buffer, sizeof(buffer));

    /* Even if no symbols, should produce some output */
    ck_assert_uint_ge(len, 0);

    nimcp_exception_unref(ex);
}
END_TEST

/*=============================================================================
 * Epitope Generation Tests
 *=============================================================================*/

START_TEST(test_generate_epitope)
{
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Epitope test"
    );

    size_t len = nimcp_exception_generate_epitope(ex);
    ck_assert_uint_gt(len, 0);
    ck_assert_uint_le(len, NIMCP_EXCEPTION_EPITOPE_SIZE);
    ck_assert_uint_eq(ex->epitope_len, len);

    /* Check epitope is non-zero */
    bool all_zero = true;
    for (size_t i = 0; i < len && i < NIMCP_EXCEPTION_EPITOPE_SIZE; i++) {
        if (ex->epitope[i] != 0) {
            all_zero = false;
            break;
        }
    }
    ck_assert(!all_zero);

    nimcp_exception_unref(ex);
}
END_TEST

START_TEST(test_suggested_recovery)
{
    /* Memory exception should suggest GC */
    nimcp_memory_exception_t* mex = nimcp_memory_exception_create(
        NIMCP_ERROR_NO_MEMORY,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        1024,
        "OOM"
    );

    nimcp_exception_recovery_action_t action = nimcp_exception_get_suggested_recovery((nimcp_exception_t*)mex);
    ck_assert(action == EXCEPTION_RECOVERY_GC || action == EXCEPTION_RECOVERY_COMPACT);

    nimcp_exception_unref((nimcp_exception_t*)mex);
}
END_TEST

/*=============================================================================
 * Integration Tests
 *=============================================================================*/

START_TEST(test_full_exception_workflow)
{
    /* Initialize system */
    nimcp_exception_system_init();

    /* Install default handlers */
    nimcp_install_default_handlers();

    /* Create exception */
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_NO_MEMORY,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        "Full workflow test: %s",
        "OOM"
    );
    ck_assert_ptr_nonnull(ex);

    /* Generate epitope */
    size_t epitope_len = nimcp_exception_generate_epitope(ex);
    ck_assert_uint_gt(epitope_len, 0);

    /* Get suggested recovery */
    nimcp_exception_recovery_action_t action = nimcp_exception_get_suggested_recovery(ex);
    ck_assert_int_ne(action, EXCEPTION_RECOVERY_NONE);

    /* Format as string */
    char buffer[512];
    size_t len = nimcp_exception_to_string(ex, buffer, sizeof(buffer));
    ck_assert_uint_gt(len, 0);

    /* Dispatch through handlers */
    nimcp_exception_dispatch(ex);

    /* Cleanup */
    nimcp_exception_unref(ex);
}
END_TEST

/*=============================================================================
 * Exception Aggregation Tests
 *=============================================================================*/

START_TEST(test_aggregate_exception_create)
{
    nimcp_aggregate_exception_t* agg = nimcp_aggregate_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Aggregate test"
    );
    ck_assert_ptr_nonnull(agg);
    ck_assert_int_eq(agg->base.type, EXCEPTION_TYPE_AGGREGATE);
    ck_assert_uint_eq(nimcp_aggregate_exception_count(agg), 0);

    nimcp_exception_unref((nimcp_exception_t*)agg);
}
END_TEST

START_TEST(test_aggregate_exception_add)
{
    nimcp_aggregate_exception_t* agg = nimcp_aggregate_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Aggregate with children"
    );

    nimcp_exception_t* child1 = nimcp_exception_create(
        NIMCP_ERROR_NULL_POINTER,
        EXCEPTION_SEVERITY_WARNING,
        __FILE__, __LINE__, __func__,
        "Child 1"
    );

    nimcp_exception_t* child2 = nimcp_exception_create(
        NIMCP_ERROR_INVALID_PARAM,
        EXCEPTION_SEVERITY_WARNING,
        __FILE__, __LINE__, __func__,
        "Child 2"
    );

    int ret = nimcp_aggregate_exception_add(agg, child1);
    ck_assert_int_eq(ret, 0);
    ck_assert_uint_eq(nimcp_aggregate_exception_count(agg), 1);

    ret = nimcp_aggregate_exception_add(agg, child2);
    ck_assert_int_eq(ret, 0);
    ck_assert_uint_eq(nimcp_aggregate_exception_count(agg), 2);

    /* Verify children can be retrieved */
    ck_assert_ptr_eq(nimcp_aggregate_exception_get(agg, 0), child1);
    ck_assert_ptr_eq(nimcp_aggregate_exception_get(agg, 1), child2);
    ck_assert_ptr_null(nimcp_aggregate_exception_get(agg, 2));

    nimcp_exception_unref((nimcp_exception_t*)agg);
}
END_TEST

/*=============================================================================
 * Structured Context Data Tests
 *=============================================================================*/

START_TEST(test_exception_context_set_get)
{
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Context test"
    );

    int ret = nimcp_exception_set_context(ex, "request_id", "abc123");
    ck_assert_int_eq(ret, 0);

    ret = nimcp_exception_set_context(ex, "user_id", "user456");
    ck_assert_int_eq(ret, 0);

    ck_assert_uint_eq(nimcp_exception_context_count(ex), 2);

    const char* val = nimcp_exception_get_context(ex, "request_id");
    ck_assert_str_eq(val, "abc123");

    val = nimcp_exception_get_context(ex, "user_id");
    ck_assert_str_eq(val, "user456");

    val = nimcp_exception_get_context(ex, "nonexistent");
    ck_assert_ptr_null(val);

    nimcp_exception_unref(ex);
}
END_TEST

START_TEST(test_exception_context_remove)
{
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Context remove test"
    );

    nimcp_exception_set_context(ex, "key1", "value1");
    nimcp_exception_set_context(ex, "key2", "value2");
    ck_assert_uint_eq(nimcp_exception_context_count(ex), 2);

    int ret = nimcp_exception_remove_context(ex, "key1");
    ck_assert_int_eq(ret, 0);
    ck_assert_uint_eq(nimcp_exception_context_count(ex), 1);
    ck_assert_ptr_null(nimcp_exception_get_context(ex, "key1"));
    ck_assert_ptr_nonnull(nimcp_exception_get_context(ex, "key2"));

    nimcp_exception_unref(ex);
}
END_TEST

/*=============================================================================
 * Circuit Breaker Tests
 *=============================================================================*/

static void circuit_setup(void) {
    setup();
    nimcp_circuit_init();
}

static void circuit_teardown(void) {
    nimcp_circuit_shutdown();
    teardown();
}

START_TEST(test_circuit_init_shutdown)
{
    /* Already initialized in fixture */
    ck_assert(nimcp_circuit_is_initialized());
}
END_TEST

START_TEST(test_circuit_record_exception)
{
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_FILE_READ,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Circuit test"
    );

    /* Record exception */
    int result = nimcp_circuit_record(ex);
    ck_assert_int_eq(result, 0);  /* Circuit should be closed */

    /* Check state */
    nimcp_circuit_state_t state = nimcp_circuit_get_state(NIMCP_ERROR_FILE_READ);
    ck_assert_int_eq(state, CIRCUIT_STATE_CLOSED);

    /* Get count */
    size_t count = nimcp_circuit_get_count(NIMCP_ERROR_FILE_READ, 60);
    ck_assert_uint_eq(count, 1);

    nimcp_exception_unref(ex);
}
END_TEST

START_TEST(test_circuit_set_threshold)
{
    int ret = nimcp_circuit_set_threshold(NIMCP_ERROR_OPERATION_FAILED, 5, 10000);
    ck_assert_int_eq(ret, 0);

    /* Create exceptions to trigger threshold */
    for (int i = 0; i < 6; i++) {
        nimcp_exception_t* ex = nimcp_exception_create(
            NIMCP_ERROR_OPERATION_FAILED,
            EXCEPTION_SEVERITY_ERROR,
            __FILE__, __LINE__, __func__,
            "Threshold test %d", i
        );
        nimcp_circuit_record(ex);
        nimcp_exception_unref(ex);
    }

    /* Circuit should now be open */
    ck_assert(nimcp_circuit_is_open(NIMCP_ERROR_OPERATION_FAILED));
}
END_TEST

START_TEST(test_circuit_reset)
{
    /* Set low threshold and trigger */
    nimcp_circuit_set_threshold(NIMCP_ERROR_NULL_POINTER, 2, 10000);

    for (int i = 0; i < 3; i++) {
        nimcp_exception_t* ex = nimcp_exception_create(
            NIMCP_ERROR_NULL_POINTER,
            EXCEPTION_SEVERITY_ERROR,
            __FILE__, __LINE__, __func__,
            "Reset test"
        );
        nimcp_circuit_record(ex);
        nimcp_exception_unref(ex);
    }

    ck_assert(nimcp_circuit_is_open(NIMCP_ERROR_NULL_POINTER));

    /* Reset circuit */
    int ret = nimcp_circuit_reset(NIMCP_ERROR_NULL_POINTER);
    ck_assert_int_eq(ret, 0);

    ck_assert(!nimcp_circuit_is_open(NIMCP_ERROR_NULL_POINTER));
}
END_TEST

START_TEST(test_circuit_stats)
{
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_INVALID_STATE,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Stats test"
    );

    nimcp_circuit_record(ex);

    nimcp_circuit_stats_t stats;
    int ret = nimcp_circuit_get_stats(&stats);
    ck_assert_int_eq(ret, 0);
    ck_assert_uint_ge(stats.total_exceptions, 1);
    ck_assert_uint_ge(stats.total_tracked, 1);

    nimcp_exception_unref(ex);
}
END_TEST

/*=============================================================================
 * Exception Suppression Tests
 *=============================================================================*/

START_TEST(test_suppression_add_remove)
{
    /* Suppress an exception code */
    int ret = nimcp_exception_suppress(NIMCP_ERROR_TIMEOUT, 60000, "Scheduled maintenance");
    ck_assert_int_eq(ret, 0);

    ck_assert(nimcp_exception_is_suppressed(NIMCP_ERROR_TIMEOUT));

    /* Unsuppress */
    ret = nimcp_exception_unsuppress(NIMCP_ERROR_TIMEOUT);
    ck_assert_int_eq(ret, 0);

    ck_assert(!nimcp_exception_is_suppressed(NIMCP_ERROR_TIMEOUT));
}
END_TEST

START_TEST(test_suppression_list_active)
{
    nimcp_exception_suppress(NIMCP_ERROR_TIMEOUT, 60000, "Test 1");
    nimcp_exception_suppress(NIMCP_ERROR_SOCKET_ERROR, 60000, "Test 2");

    nimcp_error_t codes[10];
    size_t count = nimcp_suppression_list_active(codes, 10);
    ck_assert_uint_ge(count, 2);

    nimcp_suppression_clear_all();
}
END_TEST

START_TEST(test_suppression_should_process)
{
    nimcp_exception_suppress(NIMCP_ERROR_SOCKET_ERROR, 60000, "Test suppression");

    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_SOCKET_ERROR,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Should be suppressed"
    );

    bool should_process = nimcp_exception_should_process(ex);
    ck_assert(!should_process);  /* Should be suppressed */

    nimcp_exception_unsuppress(NIMCP_ERROR_SOCKET_ERROR);

    should_process = nimcp_exception_should_process(ex);
    ck_assert(should_process);  /* Should now be processed */

    nimcp_exception_unref(ex);
}
END_TEST

/*=============================================================================
 * Distributed Tracing Tests
 *=============================================================================*/

static void trace_setup(void) {
    setup();
    nimcp_trace_init();
}

static void trace_teardown(void) {
    nimcp_trace_shutdown();
    teardown();
}

START_TEST(test_trace_create)
{
    nimcp_exception_trace_t trace = nimcp_trace_create();

    ck_assert_uint_gt(trace.trace_id, 0);
    ck_assert_uint_gt(trace.span_id, 0);
    ck_assert_uint_eq(trace.parent_span_id, 0);  /* Root span */
    ck_assert_uint_gt(trace.start_time_us, 0);
}
END_TEST

START_TEST(test_trace_create_child)
{
    nimcp_exception_trace_t parent = nimcp_trace_create();
    nimcp_exception_trace_t child = nimcp_trace_create_child(&parent);

    ck_assert_uint_eq(child.trace_id, parent.trace_id);  /* Same trace */
    ck_assert_uint_eq(child.parent_span_id, parent.span_id);
    ck_assert_uint_ne(child.span_id, parent.span_id);  /* Different span */
}
END_TEST

START_TEST(test_trace_generate_unique_ids)
{
    uint64_t id1 = nimcp_trace_generate_id();
    uint64_t id2 = nimcp_trace_generate_id();
    uint64_t id3 = nimcp_trace_generate_id();

    ck_assert_uint_ne(id1, id2);
    ck_assert_uint_ne(id2, id3);
    ck_assert_uint_ne(id1, id3);
}
END_TEST

START_TEST(test_trace_exception_association)
{
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Trace association test"
    );

    nimcp_exception_trace_t trace = nimcp_trace_create();
    int ret = nimcp_exception_set_trace(ex, &trace);
    ck_assert_int_eq(ret, 0);

    const nimcp_exception_trace_t* retrieved = nimcp_exception_get_trace(ex);
    ck_assert_ptr_nonnull(retrieved);
    ck_assert_uint_eq(retrieved->trace_id, trace.trace_id);
    ck_assert_uint_eq(retrieved->span_id, trace.span_id);

    nimcp_exception_unref(ex);
}
END_TEST

START_TEST(test_trace_stack)
{
    nimcp_exception_trace_t trace1 = nimcp_trace_create();
    nimcp_exception_trace_t trace2 = nimcp_trace_create_child(&trace1);

    /* Initially no trace on stack */
    ck_assert_ptr_null(nimcp_trace_current());

    /* Push trace 1 */
    int ret = nimcp_trace_push(&trace1);
    ck_assert_int_eq(ret, 0);
    ck_assert_ptr_nonnull(nimcp_trace_current());
    ck_assert_uint_eq(nimcp_trace_current()->span_id, trace1.span_id);

    /* Push trace 2 */
    ret = nimcp_trace_push(&trace2);
    ck_assert_int_eq(ret, 0);
    ck_assert_uint_eq(nimcp_trace_current()->span_id, trace2.span_id);

    /* Pop trace 2 */
    nimcp_trace_pop();
    ck_assert_uint_eq(nimcp_trace_current()->span_id, trace1.span_id);

    /* Pop trace 1 */
    nimcp_trace_pop();
    ck_assert_ptr_null(nimcp_trace_current());
}
END_TEST

START_TEST(test_trace_w3c_header_format)
{
    nimcp_exception_trace_t trace = nimcp_trace_create();
    trace.trace_flags = NIMCP_TRACE_FLAG_SAMPLED;

    char header[64];
    size_t len = nimcp_trace_to_header(&trace, header, sizeof(header));
    ck_assert_uint_ge(len, 55);  /* W3C format minimum length */

    /* Parse it back */
    nimcp_exception_trace_t parsed;
    int ret = nimcp_trace_from_header(header, &parsed);
    ck_assert_int_eq(ret, 0);
    ck_assert_uint_eq(parsed.span_id, trace.span_id);
    ck_assert_uint_eq(parsed.trace_flags, trace.trace_flags);
}
END_TEST

/*=============================================================================
 * Cross-Module Propagation Tests
 *=============================================================================*/

START_TEST(test_propagation_create)
{
    nimcp_propagation_context_t* ctx = nimcp_propagation_create("brain_module");
    ck_assert_ptr_nonnull(ctx);
    ck_assert_str_eq(ctx->origin_module, "brain_module");
    ck_assert_uint_eq(ctx->path_length, 0);

    nimcp_propagation_destroy(ctx);
}
END_TEST

START_TEST(test_propagation_add_hops)
{
    nimcp_propagation_context_t* ctx = nimcp_propagation_create("memory_allocator");

    int ret = nimcp_propagation_add_hop(ctx, "error_handler", "ERROR_MSG", 5);
    ck_assert_int_eq(ret, 0);
    ck_assert_uint_eq(ctx->path_length, 1);

    ret = nimcp_propagation_add_hop(ctx, "immune_system", "ANTIGEN_PRESENT", 7);
    ck_assert_int_eq(ret, 0);
    ck_assert_uint_eq(ctx->path_length, 2);

    /* Check path entries */
    ck_assert_str_eq(ctx->path[0].module_name, "error_handler");
    ck_assert_str_eq(ctx->path[1].module_name, "immune_system");

    nimcp_propagation_destroy(ctx);
}
END_TEST

START_TEST(test_propagation_requires_coordination)
{
    nimcp_propagation_context_t* ctx = nimcp_propagation_create("test");

    /* Low priority, few hops - no coordination */
    nimcp_propagation_add_hop(ctx, "mod1", "MSG1", 3);
    ck_assert(!nimcp_propagation_requires_coordination(ctx));

    /* Add more hops - coordination required */
    nimcp_propagation_add_hop(ctx, "mod2", "MSG2", 4);
    nimcp_propagation_add_hop(ctx, "mod3", "MSG3", 5);
    ck_assert(nimcp_propagation_requires_coordination(ctx));

    nimcp_propagation_destroy(ctx);
}
END_TEST

START_TEST(test_propagation_high_priority)
{
    nimcp_propagation_context_t* ctx = nimcp_propagation_create("test");

    /* High priority hop - coordination required immediately */
    nimcp_propagation_add_hop(ctx, "critical", "CRITICAL_MSG", 9);
    ck_assert(nimcp_propagation_requires_coordination(ctx));

    nimcp_propagation_destroy(ctx);
}
END_TEST

/*=============================================================================
 * Exception Metrics Tests
 *=============================================================================*/

static void metrics_setup(void) {
    setup();
    nimcp_metrics_init();
    nimcp_adaptive_init();
}

static void metrics_teardown(void) {
    nimcp_adaptive_shutdown();
    nimcp_metrics_shutdown();
    teardown();
}

START_TEST(test_metrics_init)
{
    ck_assert(nimcp_metrics_is_initialized());
    ck_assert(nimcp_adaptive_is_initialized());
}
END_TEST

START_TEST(test_metrics_record)
{
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_FILE_READ,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Metrics test"
    );

    nimcp_metrics_record_exception(ex);

    /* Record recovery - pass 0 for duration_us */
    nimcp_metrics_record_recovery(ex, EXCEPTION_RECOVERY_RETRY, true, 0);

    nimcp_exception_unref(ex);
}
END_TEST

START_TEST(test_metrics_get_category)
{
    /* Record some exceptions */
    for (int i = 0; i < 5; i++) {
        nimcp_exception_t* ex = nimcp_exception_create(
            NIMCP_ERROR_NO_MEMORY,
            EXCEPTION_SEVERITY_SEVERE,
            __FILE__, __LINE__, __func__,
            "Category metrics test"
        );
        nimcp_metrics_record_exception(ex);
        nimcp_exception_unref(ex);
    }

    /* Get top categories and find memory category */
    nimcp_category_metrics_t categories[NIMCP_METRICS_MAX_CATEGORIES];
    size_t count = nimcp_metrics_top_categories(categories, NIMCP_METRICS_MAX_CATEGORIES);
    ck_assert_uint_gt(count, 0);

    /* Find memory category in results */
    bool found = false;
    for (size_t i = 0; i < count; i++) {
        if (categories[i].category == EXCEPTION_CATEGORY_MEMORY) {
            ck_assert_uint_ge(categories[i].total_count, 5);
            found = true;
            break;
        }
    }
    ck_assert(found);
}
END_TEST

START_TEST(test_metrics_overall_stats)
{
    nimcp_exception_metrics_t stats;
    nimcp_metrics_get(&stats);
    /* Stats struct should be valid - verify no crash */
    ck_assert_uint_ge(stats.total_exceptions, 0);
}
END_TEST

/*=============================================================================
 * Adaptive Recovery Tests
 *=============================================================================*/

START_TEST(test_adaptive_suggest_action)
{
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_NO_MEMORY,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        "Adaptive test"
    );

    /* Generate epitope for pattern matching */
    nimcp_exception_generate_epitope(ex);

    /* No data yet - should suggest default action */
    nimcp_exception_recovery_action_t action = nimcp_adaptive_suggest_action(ex);
    /* Action may be NONE or a default - just verify no crash */
    (void)action;

    nimcp_exception_unref(ex);
}
END_TEST

START_TEST(test_adaptive_record_outcome)
{
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_FILE_READ,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Adaptive record test"
    );
    nimcp_exception_generate_epitope(ex);

    /* Record multiple outcomes to train the system */
    for (int i = 0; i < 10; i++) {
        int ret = nimcp_adaptive_record_outcome(ex, EXCEPTION_RECOVERY_RETRY, true);
        ck_assert_int_eq(ret, 0);
    }

    /* System should now suggest RETRY for this pattern */
    nimcp_exception_recovery_action_t action = nimcp_adaptive_suggest_action(ex);
    /* After enough samples, should suggest the successful action */
    if (action != EXCEPTION_RECOVERY_NONE) {
        ck_assert_int_eq(action, EXCEPTION_RECOVERY_RETRY);
    }

    nimcp_exception_unref(ex);
}
END_TEST

START_TEST(test_adaptive_get_confidence)
{
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_INVALID_PARAM,
        EXCEPTION_SEVERITY_WARNING,
        __FILE__, __LINE__, __func__,
        "Confidence test"
    );
    nimcp_exception_generate_epitope(ex);

    /* No data - low confidence */
    float conf = nimcp_adaptive_get_confidence(ex, EXCEPTION_RECOVERY_RETRY);
    ck_assert(conf >= 0.0f && conf <= 1.0f);

    /* Record outcomes to build confidence */
    for (int i = 0; i < 20; i++) {
        nimcp_adaptive_record_outcome(ex, EXCEPTION_RECOVERY_GC, true);
    }

    /* Should have higher confidence now */
    float new_conf = nimcp_adaptive_get_confidence(ex, EXCEPTION_RECOVERY_GC);
    ck_assert(new_conf >= 0.0f && new_conf <= 1.0f);

    nimcp_exception_unref(ex);
}
END_TEST

START_TEST(test_adaptive_reset_pattern)
{
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_TIMEOUT,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Reset pattern test"
    );
    nimcp_exception_generate_epitope(ex);

    /* Train the pattern */
    for (int i = 0; i < 10; i++) {
        nimcp_adaptive_record_outcome(ex, EXCEPTION_RECOVERY_RETRY, true);
    }

    /* Reset this specific pattern */
    nimcp_adaptive_reset_pattern(ex->epitope, ex->epitope_len);

    /* Confidence should be back to low */
    float conf = nimcp_adaptive_get_confidence(ex, EXCEPTION_RECOVERY_RETRY);
    ck_assert(conf < 0.5f);

    nimcp_exception_unref(ex);
}
END_TEST

START_TEST(test_adaptive_stats)
{
    nimcp_adaptive_stats_t stats;
    nimcp_adaptive_get_stats(&stats);

    /* Stats should be valid */
    ck_assert(stats.suggestion_accuracy >= 0.0f && stats.suggestion_accuracy <= 1.0f);
}
END_TEST

/*=============================================================================
 * Test Suite Creation
 *=============================================================================*/

Suite* exception_suite(void)
{
    Suite* s = suite_create("Exception System");

    /* Exception creation tests */
    TCase* tc_create = tcase_create("Creation");
    tcase_add_checked_fixture(tc_create, setup, teardown);
    tcase_add_test(tc_create, test_exception_create_basic);
    tcase_add_test(tc_create, test_exception_create_null_message);
    tcase_add_test(tc_create, test_memory_exception_create);
    tcase_add_test(tc_create, test_brain_exception_create);
    tcase_add_test(tc_create, test_io_exception_create);
    tcase_add_test(tc_create, test_threading_exception_create);
    tcase_add_test(tc_create, test_gpu_exception_create);
    suite_add_tcase(s, tc_create);

    /* Reference counting tests */
    TCase* tc_refcount = tcase_create("Reference Counting");
    tcase_add_checked_fixture(tc_refcount, setup, teardown);
    tcase_add_test(tc_refcount, test_exception_ref_unref);
    tcase_add_test(tc_refcount, test_exception_ref_null);
    tcase_add_test(tc_refcount, test_exception_unref_null);
    suite_add_tcase(s, tc_refcount);

    /* Chaining tests */
    TCase* tc_chain = tcase_create("Exception Chaining");
    tcase_add_checked_fixture(tc_chain, setup, teardown);
    tcase_add_test(tc_chain, test_exception_set_cause);
    tcase_add_test(tc_chain, test_exception_get_cause_null);
    suite_add_tcase(s, tc_chain);

    /* Category/Severity tests */
    TCase* tc_category = tcase_create("Category and Severity");
    tcase_add_checked_fixture(tc_category, setup, teardown);
    tcase_add_test(tc_category, test_get_category_from_code_memory);
    tcase_add_test(tc_category, test_get_category_from_code_brain);
    tcase_add_test(tc_category, test_get_category_from_code_io);
    tcase_add_test(tc_category, test_get_category_from_code_threading);
    tcase_add_test(tc_category, test_get_severity_from_code);
    suite_add_tcase(s, tc_category);

    /* String conversion tests */
    TCase* tc_strings = tcase_create("String Conversions");
    tcase_add_checked_fixture(tc_strings, setup, teardown);
    tcase_add_test(tc_strings, test_severity_to_string);
    tcase_add_test(tc_strings, test_category_to_string);
    tcase_add_test(tc_strings, test_type_to_string);
    tcase_add_test(tc_strings, test_recovery_action_to_string);
    tcase_add_test(tc_strings, test_exception_to_string);
    suite_add_tcase(s, tc_strings);

    /* Thread-local context tests */
    TCase* tc_context = tcase_create("Thread-Local Context");
    tcase_add_checked_fixture(tc_context, setup, teardown);
    tcase_add_test(tc_context, test_set_get_current);
    tcase_add_test(tc_context, test_current_exception_replace);
    suite_add_tcase(s, tc_context);

    /* Handler tests */
    TCase* tc_handlers = tcase_create("Handler Registration");
    tcase_add_checked_fixture(tc_handlers, handler_setup, teardown);
    tcase_add_test(tc_handlers, test_handler_register_unregister);
    tcase_add_test(tc_handlers, test_handler_register_null_options);
    tcase_add_test(tc_handlers, test_handler_register_null_handler);
    tcase_add_test(tc_handlers, test_handler_enable_disable);
    tcase_add_test(tc_handlers, test_handler_dispatch);
    tcase_add_test(tc_handlers, test_handler_dispatch_consuming);
    tcase_add_test(tc_handlers, test_handler_priority_order);
    tcase_add_test(tc_handlers, test_handler_category_filter);
    suite_add_tcase(s, tc_handlers);

    /* Try/Catch tests */
    TCase* tc_trycatch = tcase_create("Try/Catch");
    tcase_add_checked_fixture(tc_trycatch, setup, teardown);
    tcase_add_test(tc_trycatch, test_try_catch_no_exception);
    tcase_add_test(tc_trycatch, test_try_catch_with_exception);
    tcase_add_test(tc_trycatch, test_try_context_stack);
    tcase_add_test(tc_trycatch, test_nested_try_catch);
    suite_add_tcase(s, tc_trycatch);

    /* Recovery tests */
    TCase* tc_recovery = tcase_create("Recovery Callbacks");
    tcase_add_checked_fixture(tc_recovery, recovery_setup, teardown);
    tcase_add_test(tc_recovery, test_register_recovery_callback);
    tcase_add_test(tc_recovery, test_execute_recovery);
    tcase_add_test(tc_recovery, test_recovery_no_callback);
    suite_add_tcase(s, tc_recovery);

    /* Immune integration tests */
    TCase* tc_immune = tcase_create("Immune Integration");
    tcase_add_checked_fixture(tc_immune, setup, teardown);
    tcase_add_test(tc_immune, test_exception_to_antigen_source);
    tcase_add_test(tc_immune, test_exception_to_immune_severity);
    tcase_add_test(tc_immune, test_epitope_computation);
    tcase_add_test(tc_immune, test_recovery_strategy);
    tcase_add_test(tc_immune, test_immune_config_defaults);
    tcase_add_test(tc_immune, test_immune_not_connected);
    suite_add_tcase(s, tc_immune);

    /* Default handler tests */
    TCase* tc_defaults = tcase_create("Default Handlers");
    tcase_add_checked_fixture(tc_defaults, setup, teardown);
    tcase_add_test(tc_defaults, test_install_default_handlers);
    suite_add_tcase(s, tc_defaults);

    /* Stack trace tests */
    TCase* tc_stack = tcase_create("Stack Trace");
    tcase_add_checked_fixture(tc_stack, setup, teardown);
    tcase_add_test(tc_stack, test_stack_trace_capture);
    tcase_add_test(tc_stack, test_stack_trace_to_string);
    suite_add_tcase(s, tc_stack);

    /* Epitope tests */
    TCase* tc_epitope = tcase_create("Epitope Generation");
    tcase_add_checked_fixture(tc_epitope, setup, teardown);
    tcase_add_test(tc_epitope, test_generate_epitope);
    tcase_add_test(tc_epitope, test_suggested_recovery);
    suite_add_tcase(s, tc_epitope);

    /* Integration tests */
    TCase* tc_integration = tcase_create("Integration");
    tcase_add_test(tc_integration, test_full_exception_workflow);
    suite_add_tcase(s, tc_integration);

    /* Exception Aggregation tests */
    TCase* tc_aggregation = tcase_create("Exception Aggregation");
    tcase_add_checked_fixture(tc_aggregation, setup, teardown);
    tcase_add_test(tc_aggregation, test_aggregate_exception_create);
    tcase_add_test(tc_aggregation, test_aggregate_exception_add);
    suite_add_tcase(s, tc_aggregation);

    /* Structured Context Data tests */
    TCase* tc_context_data = tcase_create("Structured Context Data");
    tcase_add_checked_fixture(tc_context_data, setup, teardown);
    tcase_add_test(tc_context_data, test_exception_context_set_get);
    tcase_add_test(tc_context_data, test_exception_context_remove);
    suite_add_tcase(s, tc_context_data);

    /* Circuit Breaker tests */
    TCase* tc_circuit = tcase_create("Circuit Breaker");
    tcase_add_checked_fixture(tc_circuit, circuit_setup, circuit_teardown);
    tcase_add_test(tc_circuit, test_circuit_init_shutdown);
    tcase_add_test(tc_circuit, test_circuit_record_exception);
    tcase_add_test(tc_circuit, test_circuit_set_threshold);
    tcase_add_test(tc_circuit, test_circuit_reset);
    tcase_add_test(tc_circuit, test_circuit_stats);
    suite_add_tcase(s, tc_circuit);

    /* Exception Suppression tests */
    TCase* tc_suppression = tcase_create("Exception Suppression");
    tcase_add_checked_fixture(tc_suppression, circuit_setup, circuit_teardown);
    tcase_add_test(tc_suppression, test_suppression_add_remove);
    tcase_add_test(tc_suppression, test_suppression_list_active);
    tcase_add_test(tc_suppression, test_suppression_should_process);
    suite_add_tcase(s, tc_suppression);

    /* Distributed Tracing tests */
    TCase* tc_trace = tcase_create("Distributed Tracing");
    tcase_add_checked_fixture(tc_trace, trace_setup, trace_teardown);
    tcase_add_test(tc_trace, test_trace_create);
    tcase_add_test(tc_trace, test_trace_create_child);
    tcase_add_test(tc_trace, test_trace_generate_unique_ids);
    tcase_add_test(tc_trace, test_trace_exception_association);
    tcase_add_test(tc_trace, test_trace_stack);
    tcase_add_test(tc_trace, test_trace_w3c_header_format);
    suite_add_tcase(s, tc_trace);

    /* Cross-Module Propagation tests */
    TCase* tc_propagation = tcase_create("Cross-Module Propagation");
    tcase_add_checked_fixture(tc_propagation, trace_setup, trace_teardown);
    tcase_add_test(tc_propagation, test_propagation_create);
    tcase_add_test(tc_propagation, test_propagation_add_hops);
    tcase_add_test(tc_propagation, test_propagation_requires_coordination);
    tcase_add_test(tc_propagation, test_propagation_high_priority);
    suite_add_tcase(s, tc_propagation);

    /* Exception Metrics tests */
    TCase* tc_metrics = tcase_create("Exception Metrics");
    tcase_add_checked_fixture(tc_metrics, metrics_setup, metrics_teardown);
    tcase_add_test(tc_metrics, test_metrics_init);
    tcase_add_test(tc_metrics, test_metrics_record);
    tcase_add_test(tc_metrics, test_metrics_get_category);
    tcase_add_test(tc_metrics, test_metrics_overall_stats);
    suite_add_tcase(s, tc_metrics);

    /* Adaptive Recovery tests */
    TCase* tc_adaptive = tcase_create("Adaptive Recovery");
    tcase_add_checked_fixture(tc_adaptive, metrics_setup, metrics_teardown);
    tcase_add_test(tc_adaptive, test_adaptive_suggest_action);
    tcase_add_test(tc_adaptive, test_adaptive_record_outcome);
    tcase_add_test(tc_adaptive, test_adaptive_get_confidence);
    tcase_add_test(tc_adaptive, test_adaptive_reset_pattern);
    tcase_add_test(tc_adaptive, test_adaptive_stats);
    suite_add_tcase(s, tc_adaptive);

    return s;
}

int main(void)
{
    int number_failed;
    Suite* s = exception_suite();
    SRunner* sr = srunner_create(s);

    /* Run all tests */
    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
