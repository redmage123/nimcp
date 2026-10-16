/**
 * @file test_signal_exception.c
 * @brief Unit tests for signal exception integration
 *
 * WHAT: Test suite for signal-to-exception bridging
 * WHY:  Verify correct behavior of signal exception queue and exception creation
 * HOW:  Unit tests using Check framework
 *
 * @author NIMCP Development Team
 * @date 2026-01-16
 */

#include <check.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_handlers.h"
#include "utils/exception/nimcp_exception_immune.h"
#include "utils/signal/nimcp_signal_handler.h"
#include "utils/signal/nimcp_signal_exception_queue.h"

/*=============================================================================
 * Test Fixtures
 *=============================================================================*/

static void setup(void)
{
    nimcp_exception_system_init();
    signal_exception_queue_init();
}

static void teardown(void)
{
    signal_exception_queue_shutdown();
    nimcp_exception_clear_current();
    nimcp_exception_system_shutdown();
}

/*=============================================================================
 * Signal Exception Creation Tests
 *=============================================================================*/

START_TEST(test_signal_exception_create_basic)
{
    nimcp_signal_exception_t* ex = nimcp_signal_exception_create(
        SIGSEGV,
        (void*)0xDEADBEEF,
        __FILE__,
        __LINE__,
        __func__,
        "Test SIGSEGV at address %p",
        (void*)0xDEADBEEF
    );
    ck_assert_ptr_nonnull(ex);
    ck_assert_int_eq(ex->base.type, EXCEPTION_TYPE_SIGNAL);
    ck_assert_int_eq(ex->base.category, EXCEPTION_CATEGORY_SIGNAL);
    ck_assert_int_eq(ex->base.code, NIMCP_ERROR_SIGSEGV);
    ck_assert_int_eq(ex->base.severity, EXCEPTION_SEVERITY_FATAL);
    ck_assert_int_eq(ex->signal_number, SIGSEGV);
    ck_assert_ptr_eq(ex->fault_address, (void*)0xDEADBEEF);
    ck_assert(strstr(ex->base.message, "SIGSEGV") != NULL ||
              strstr(ex->base.message, "0xdeadbeef") != NULL);

    nimcp_exception_unref((nimcp_exception_t*)ex);
}
END_TEST

START_TEST(test_signal_exception_create_sigfpe)
{
    nimcp_signal_exception_t* ex = nimcp_signal_exception_create(
        SIGFPE,
        NULL,
        __FILE__,
        __LINE__,
        __func__,
        "Division by zero"
    );
    ck_assert_ptr_nonnull(ex);
    ck_assert_int_eq(ex->signal_number, SIGFPE);
    ck_assert_int_eq(ex->base.code, NIMCP_ERROR_SIGFPE);
    ck_assert_ptr_null(ex->fault_address);

    nimcp_exception_unref((nimcp_exception_t*)ex);
}
END_TEST

START_TEST(test_signal_exception_create_sigbus)
{
    nimcp_signal_exception_t* ex = nimcp_signal_exception_create(
        SIGBUS,
        (void*)0x1234,
        __FILE__,
        __LINE__,
        __func__,
        NULL
    );
    ck_assert_ptr_nonnull(ex);
    ck_assert_int_eq(ex->signal_number, SIGBUS);
    ck_assert_int_eq(ex->base.code, NIMCP_ERROR_SIGBUS);
    /* Default message should be generated */
    ck_assert(ex->base.message[0] != '\0');

    nimcp_exception_unref((nimcp_exception_t*)ex);
}
END_TEST

START_TEST(test_signal_exception_create_from_context)
{
    signal_crash_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.signal = SIGSEGV;
    ctx.fault_address = (void*)0xBADCAFE;
    ctx.instruction_pointer = (void*)0x400000;
    ctx.stack_pointer = (void*)0x7FFF1234;
    ctx.base_pointer = (void*)0x7FFF1200;
    strncpy(ctx.memory_region, "/lib/libc.so.6 [r-xp]", sizeof(ctx.memory_region) - 1);
    ctx.backtrace_depth = 0;

    nimcp_signal_exception_t* ex = nimcp_signal_exception_create_from_context(&ctx);
    ck_assert_ptr_nonnull(ex);
    ck_assert_int_eq(ex->signal_number, SIGSEGV);
    ck_assert_ptr_eq(ex->fault_address, (void*)0xBADCAFE);
    ck_assert_ptr_eq(ex->instruction_pointer, (void*)0x400000);
    ck_assert_ptr_eq(ex->stack_pointer, (void*)0x7FFF1234);
    ck_assert_ptr_eq(ex->base_pointer, (void*)0x7FFF1200);
    ck_assert(strstr(ex->memory_region, "libc.so") != NULL);
    ck_assert_int_eq(ex->siglongjmp_executed, false);
    ck_assert_int_eq(ex->recovery_attempted, false);

    nimcp_exception_unref((nimcp_exception_t*)ex);
}
END_TEST

START_TEST(test_signal_exception_create_from_null_context)
{
    nimcp_signal_exception_t* ex = nimcp_signal_exception_create_from_context(NULL);
    ck_assert_ptr_null(ex);
}
END_TEST

/*=============================================================================
 * Signal to Error Code Mapping Tests
 *=============================================================================*/

START_TEST(test_signal_to_error_code_sigsegv)
{
    nimcp_error_t code = nimcp_signal_to_error_code(SIGSEGV);
    ck_assert_int_eq(code, NIMCP_ERROR_SIGSEGV);
}
END_TEST

START_TEST(test_signal_to_error_code_sigabrt)
{
    nimcp_error_t code = nimcp_signal_to_error_code(SIGABRT);
    ck_assert_int_eq(code, NIMCP_ERROR_SIGABRT);
}
END_TEST

START_TEST(test_signal_to_error_code_sigfpe)
{
    nimcp_error_t code = nimcp_signal_to_error_code(SIGFPE);
    ck_assert_int_eq(code, NIMCP_ERROR_SIGFPE);
}
END_TEST

START_TEST(test_signal_to_error_code_sigbus)
{
    nimcp_error_t code = nimcp_signal_to_error_code(SIGBUS);
    ck_assert_int_eq(code, NIMCP_ERROR_SIGBUS);
}
END_TEST

START_TEST(test_signal_to_error_code_sigill)
{
    nimcp_error_t code = nimcp_signal_to_error_code(SIGILL);
    ck_assert_int_eq(code, NIMCP_ERROR_SIGILL);
}
END_TEST

START_TEST(test_signal_to_error_code_unknown)
{
    nimcp_error_t code = nimcp_signal_to_error_code(999);
    ck_assert_int_eq(code, NIMCP_ERROR_SIGNAL_RECEIVED);
}
END_TEST

/*=============================================================================
 * Signal Name Tests
 *=============================================================================*/

START_TEST(test_signal_name_sigsegv)
{
    const char* name = nimcp_signal_name(SIGSEGV);
    ck_assert_str_eq(name, "SIGSEGV");
}
END_TEST

START_TEST(test_signal_name_sigfpe)
{
    const char* name = nimcp_signal_name(SIGFPE);
    ck_assert_str_eq(name, "SIGFPE");
}
END_TEST

START_TEST(test_signal_name_unknown)
{
    const char* name = nimcp_signal_name(999);
    ck_assert_str_eq(name, "UNKNOWN");
}
END_TEST

/*=============================================================================
 * Exception Queue Tests
 *=============================================================================*/

START_TEST(test_queue_init_shutdown)
{
    /* Queue already initialized in setup */
    ck_assert(signal_exception_queue_is_initialized());

    /* Check it's empty */
    ck_assert(signal_exception_queue_is_empty());
    ck_assert_uint_eq(signal_exception_queue_pending_count(), 0);
}
END_TEST

START_TEST(test_queue_enqueue_dequeue)
{
    signal_crash_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.signal = SIGSEGV;
    ctx.fault_address = (void*)0x12345678;

    /* Enqueue */
    bool success = signal_exception_queue_enqueue(SIGSEGV, &ctx);
    ck_assert(success);
    ck_assert_uint_eq(signal_exception_queue_pending_count(), 1);
    ck_assert(!signal_exception_queue_is_empty());

    /* Dequeue */
    signal_exception_entry_t entry;
    success = signal_exception_queue_dequeue(&entry);
    ck_assert(success);
    ck_assert_int_eq(entry.ctx.signal, SIGSEGV);
    ck_assert_ptr_eq(entry.ctx.fault_address, (void*)0x12345678);
    ck_assert_uint_gt(entry.timestamp_us, 0);

    /* Should be empty now */
    ck_assert(signal_exception_queue_is_empty());
    ck_assert_uint_eq(signal_exception_queue_pending_count(), 0);
}
END_TEST

START_TEST(test_queue_dequeue_empty)
{
    signal_exception_entry_t entry;
    bool success = signal_exception_queue_dequeue(&entry);
    ck_assert(!success);
}
END_TEST

START_TEST(test_queue_multiple_entries)
{
    signal_crash_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));

    /* Enqueue multiple entries */
    for (int i = 0; i < 5; i++) {
        ctx.signal = SIGSEGV + i;
        ctx.fault_address = (void*)(uintptr_t)(0x1000 + i);
        bool success = signal_exception_queue_enqueue(ctx.signal, &ctx);
        ck_assert(success);
    }
    ck_assert_uint_eq(signal_exception_queue_pending_count(), 5);

    /* Dequeue and verify order (FIFO) */
    for (int i = 0; i < 5; i++) {
        signal_exception_entry_t entry;
        bool success = signal_exception_queue_dequeue(&entry);
        ck_assert(success);
        ck_assert_int_eq(entry.ctx.signal, SIGSEGV + i);
        ck_assert_ptr_eq(entry.ctx.fault_address, (void*)(uintptr_t)(0x1000 + i));
    }

    ck_assert(signal_exception_queue_is_empty());
}
END_TEST

START_TEST(test_queue_statistics)
{
    signal_crash_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.signal = SIGSEGV;

    /* Reset stats */
    signal_exception_queue_reset_stats();

    /* Enqueue some entries */
    for (int i = 0; i < 3; i++) {
        signal_exception_queue_enqueue(SIGSEGV, &ctx);
    }

    /* Dequeue one */
    signal_exception_entry_t entry;
    signal_exception_queue_dequeue(&entry);

    /* Check stats */
    signal_exception_queue_stats_t stats;
    signal_exception_queue_get_stats(&stats);
    ck_assert_uint_eq(stats.enqueue_count, 3);
    ck_assert_uint_eq(stats.dequeue_count, 1);
    ck_assert_uint_eq(stats.pending_count, 2);
    ck_assert_uint_eq(stats.queue_capacity, SIGNAL_EXCEPTION_QUEUE_SIZE);
}
END_TEST

/*=============================================================================
 * Exception Type String Tests
 *=============================================================================*/

START_TEST(test_exception_type_to_string_signal)
{
    const char* name = nimcp_exception_type_to_string(EXCEPTION_TYPE_SIGNAL);
    ck_assert_str_eq(name, "SIGNAL");
}
END_TEST

/*=============================================================================
 * Recovery Strategy Tests
 *=============================================================================*/

START_TEST(test_signal_recovery_strategy)
{
    nimcp_signal_exception_t* ex = nimcp_signal_exception_create(
        SIGSEGV,
        NULL,
        __FILE__,
        __LINE__,
        __func__,
        "Test SIGSEGV"
    );
    ck_assert_ptr_nonnull(ex);

    nimcp_exception_recovery_strategy_t strategy;
    nimcp_exception_get_recovery_strategy((nimcp_exception_t*)ex, &strategy);

    /* SIGNAL category should use EMERGENCY_SAVE as primary action */
    ck_assert_int_eq(strategy.primary_action, EXCEPTION_RECOVERY_EMERGENCY_SAVE);
    ck_assert_int_eq(strategy.fallback_action, EXCEPTION_RECOVERY_GRACEFUL_SHUTDOWN);
    ck_assert_int_eq(strategy.retry_count, 1);
    ck_assert_int_eq(strategy.cooldown_ms, 0);

    nimcp_exception_unref((nimcp_exception_t*)ex);
}
END_TEST

/*=============================================================================
 * Test Suite
 *=============================================================================*/

Suite* signal_exception_suite(void)
{
    Suite* s = suite_create("SignalException");

    /* Signal Exception Creation */
    TCase* tc_create = tcase_create("Creation");
    tcase_add_checked_fixture(tc_create, setup, teardown);
    tcase_add_test(tc_create, test_signal_exception_create_basic);
    tcase_add_test(tc_create, test_signal_exception_create_sigfpe);
    tcase_add_test(tc_create, test_signal_exception_create_sigbus);
    tcase_add_test(tc_create, test_signal_exception_create_from_context);
    tcase_add_test(tc_create, test_signal_exception_create_from_null_context);
    suite_add_tcase(s, tc_create);

    /* Error Code Mapping */
    TCase* tc_codes = tcase_create("ErrorCodes");
    tcase_add_checked_fixture(tc_codes, setup, teardown);
    tcase_add_test(tc_codes, test_signal_to_error_code_sigsegv);
    tcase_add_test(tc_codes, test_signal_to_error_code_sigabrt);
    tcase_add_test(tc_codes, test_signal_to_error_code_sigfpe);
    tcase_add_test(tc_codes, test_signal_to_error_code_sigbus);
    tcase_add_test(tc_codes, test_signal_to_error_code_sigill);
    tcase_add_test(tc_codes, test_signal_to_error_code_unknown);
    suite_add_tcase(s, tc_codes);

    /* Signal Names */
    TCase* tc_names = tcase_create("SignalNames");
    tcase_add_checked_fixture(tc_names, setup, teardown);
    tcase_add_test(tc_names, test_signal_name_sigsegv);
    tcase_add_test(tc_names, test_signal_name_sigfpe);
    tcase_add_test(tc_names, test_signal_name_unknown);
    suite_add_tcase(s, tc_names);

    /* Queue Operations */
    TCase* tc_queue = tcase_create("Queue");
    tcase_add_checked_fixture(tc_queue, setup, teardown);
    tcase_add_test(tc_queue, test_queue_init_shutdown);
    tcase_add_test(tc_queue, test_queue_enqueue_dequeue);
    tcase_add_test(tc_queue, test_queue_dequeue_empty);
    tcase_add_test(tc_queue, test_queue_multiple_entries);
    tcase_add_test(tc_queue, test_queue_statistics);
    suite_add_tcase(s, tc_queue);

    /* Type String */
    TCase* tc_typestr = tcase_create("TypeString");
    tcase_add_checked_fixture(tc_typestr, setup, teardown);
    tcase_add_test(tc_typestr, test_exception_type_to_string_signal);
    suite_add_tcase(s, tc_typestr);

    /* Recovery Strategy */
    TCase* tc_recovery = tcase_create("Recovery");
    tcase_add_checked_fixture(tc_recovery, setup, teardown);
    tcase_add_test(tc_recovery, test_signal_recovery_strategy);
    suite_add_tcase(s, tc_recovery);

    return s;
}

int main(void)
{
    int number_failed;
    Suite* s = signal_exception_suite();
    SRunner* sr = srunner_create(s);

    srunner_set_fork_status(sr, CK_NOFORK);
    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
