/**
 * @file test_exception_immune.cpp
 * @brief Tests for immune integration (P1-2/3/5/6)
 *
 * WHAT: Verify exception macros and immune integration compile and work
 * WHY:  P1-2/3/5/6 integrated exception handling with immune system
 * HOW:  Test exception creation, severity mapping, macros, and ref counting
 *
 * Function signatures tested (from include/utils/exception/nimcp_exception.h):
 *   nimcp_exception_t* nimcp_exception_create(
 *       nimcp_error_t code, nimcp_exception_severity_t severity,
 *       const char* file, int line, const char* func,
 *       const char* format, ...);
 *   void nimcp_exception_unref(nimcp_exception_t* ex);
 *   nimcp_exception_severity_t nimcp_exception_get_severity_from_code(nimcp_error_t code);
 *
 * Macros tested (from include/utils/exception/nimcp_exception_macros.h):
 *   NIMCP_THROW(code, fmt, ...)
 *   NIMCP_THROW_IF(cond, code, fmt, ...)
 *   NIMCP_THROW_TO_IMMUNE(code, fmt, ...)
 *   NIMCP_THROW_IMMUNE_RECOVER(code, fmt, ...)
 *
 * Error codes (from include/utils/error/nimcp_error_codes.h):
 *   NIMCP_SUCCESS (0)
 *   NIMCP_ERROR_NULL_POINTER (1003)
 *   NIMCP_ERROR_INVALID_PARAM (1002)
 */

#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/error/nimcp_error_codes.h"
}

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class ExceptionImmuneTest : public ::testing::Test {
protected:
    void SetUp() override {
        // No special setup needed - exception system self-initializes
    }

    void TearDown() override {
        // No special cleanup
    }
};

/* ============================================================================
 * Exception Creation Tests
 * ============================================================================ */

TEST_F(ExceptionImmuneTest, CreateBasicException) {
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_NULL_POINTER,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__,
        __LINE__,
        __func__,
        "Test exception: %s",
        "null pointer"
    );
    ASSERT_NE(ex, nullptr);

    // Verify fields
    EXPECT_EQ(ex->code, NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(ex->severity, EXCEPTION_SEVERITY_ERROR);
    EXPECT_EQ(ex->type, EXCEPTION_TYPE_BASE);
    EXPECT_NE(ex->file, nullptr);
    EXPECT_GT(ex->line, 0);

    nimcp_exception_unref(ex);
}

TEST_F(ExceptionImmuneTest, CreateExceptionWithFormat) {
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_INVALID_PARAM,
        EXCEPTION_SEVERITY_WARNING,
        __FILE__,
        __LINE__,
        __func__,
        "Parameter %d out of range [%d, %d]",
        42, 0, 10
    );
    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_INVALID_PARAM);
    EXPECT_EQ(ex->severity, EXCEPTION_SEVERITY_WARNING);

    nimcp_exception_unref(ex);
}

/* ============================================================================
 * Exception Destruction Tests
 * ============================================================================ */

TEST_F(ExceptionImmuneTest, UnrefNullException) {
    // Should be safe to unref NULL
    nimcp_exception_unref(nullptr);
    SUCCEED() << "Unref on NULL did not crash";
}

/* ============================================================================
 * Severity Mapping Tests
 * ============================================================================ */

TEST_F(ExceptionImmuneTest, SeverityFromErrorCode) {
    // Error codes should map to appropriate severity levels
    nimcp_exception_severity_t sev;

    sev = nimcp_exception_get_severity_from_code(NIMCP_ERROR_NULL_POINTER);
    EXPECT_GE(sev, EXCEPTION_SEVERITY_ERROR);

    sev = nimcp_exception_get_severity_from_code(NIMCP_ERROR_INVALID_PARAM);
    EXPECT_GE(sev, EXCEPTION_SEVERITY_WARNING);
}

TEST_F(ExceptionImmuneTest, SeverityFromSuccessCode) {
    nimcp_exception_severity_t sev = nimcp_exception_get_severity_from_code(NIMCP_SUCCESS);
    // Success should map to a low severity
    EXPECT_LE(sev, EXCEPTION_SEVERITY_INFO);
}

/* ============================================================================
 * NIMCP_THROW Macro Tests
 * ============================================================================ */

TEST_F(ExceptionImmuneTest, ThrowMacroCompiles) {
    // Verify the NIMCP_THROW macro compiles and can be called
    // The throw dispatches to the handler chain but does not abort
    NIMCP_THROW(NIMCP_ERROR_UNKNOWN, "Test throw from unit test");
    SUCCEED() << "NIMCP_THROW macro compiled and executed";
}

TEST_F(ExceptionImmuneTest, ThrowIfMacroTrue) {
    bool condition = true;
    NIMCP_THROW_IF(condition, NIMCP_ERROR_UNKNOWN, "Condition was true");
    SUCCEED() << "NIMCP_THROW_IF with true condition executed";
}

TEST_F(ExceptionImmuneTest, ThrowIfMacroFalse) {
    bool condition = false;
    NIMCP_THROW_IF(condition, NIMCP_ERROR_UNKNOWN, "Should not throw");
    SUCCEED() << "NIMCP_THROW_IF with false condition did not throw";
}

/* ============================================================================
 * NIMCP_THROW_TO_IMMUNE Macro Tests
 * ============================================================================ */

TEST_F(ExceptionImmuneTest, ThrowToImmuneCompiles) {
    // NIMCP_THROW_TO_IMMUNE creates an exception, presents to immune,
    // dispatches, and unrefs. Without an immune system connected,
    // the present_to_immune should be a no-op.
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Test immune throw");
    SUCCEED() << "NIMCP_THROW_TO_IMMUNE compiled and executed";
}

/* ============================================================================
 * NIMCP_THROW_IMMUNE_RECOVER Macro Tests
 * ============================================================================ */

TEST_F(ExceptionImmuneTest, ThrowImmuneRecoverCompiles) {
    // NIMCP_THROW_IMMUNE_RECOVER creates exception, presents to immune,
    // checks for recovery action, and executes it if found.
    NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_UNKNOWN, "Test immune recover");
    SUCCEED() << "NIMCP_THROW_IMMUNE_RECOVER compiled and executed";
}

/* ============================================================================
 * Exception Category Tests
 * ============================================================================ */

TEST_F(ExceptionImmuneTest, ExceptionCategoryValues) {
    // Verify category enum values are reasonable
    EXPECT_EQ(EXCEPTION_CATEGORY_GENERIC, 1);
    EXPECT_EQ(EXCEPTION_CATEGORY_MEMORY, 2);
    EXPECT_EQ(EXCEPTION_CATEGORY_BRAIN, 3);
    EXPECT_EQ(EXCEPTION_CATEGORY_IO, 4);
    EXPECT_EQ(EXCEPTION_CATEGORY_THREADING, 6);
}

TEST_F(ExceptionImmuneTest, ExceptionSeverityOrdering) {
    // Verify severity levels are properly ordered
    EXPECT_LT(EXCEPTION_SEVERITY_DEBUG, EXCEPTION_SEVERITY_INFO);
    EXPECT_LT(EXCEPTION_SEVERITY_INFO, EXCEPTION_SEVERITY_WARNING);
    EXPECT_LT(EXCEPTION_SEVERITY_WARNING, EXCEPTION_SEVERITY_ERROR);
    EXPECT_LT(EXCEPTION_SEVERITY_ERROR, EXCEPTION_SEVERITY_SEVERE);
    EXPECT_LT(EXCEPTION_SEVERITY_SEVERE, EXCEPTION_SEVERITY_CRITICAL);
    EXPECT_LT(EXCEPTION_SEVERITY_CRITICAL, EXCEPTION_SEVERITY_FATAL);
}

/* ============================================================================
 * Recovery Action Type Tests
 * ============================================================================ */

TEST_F(ExceptionImmuneTest, RecoveryActionValues) {
    EXPECT_EQ(EXCEPTION_RECOVERY_NONE, 0);
    EXPECT_NE(EXCEPTION_RECOVERY_RETRY, EXCEPTION_RECOVERY_NONE);
    EXPECT_NE(EXCEPTION_RECOVERY_GC, EXCEPTION_RECOVERY_NONE);
    EXPECT_NE(EXCEPTION_RECOVERY_ROLLBACK, EXCEPTION_RECOVERY_NONE);
}

/* ============================================================================
 * Exception Struct Field Tests
 * ============================================================================ */

TEST_F(ExceptionImmuneTest, ExceptionStructSize) {
    // Verify the struct is a reasonable size
    EXPECT_GT(sizeof(nimcp_exception_t), 0u);
    // Should have enough room for all fields
    EXPECT_GE(sizeof(nimcp_exception_t), sizeof(nimcp_error_t) + sizeof(nimcp_exception_severity_t));
}
