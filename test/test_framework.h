/**
 * @file test_framework.h
 * @brief Minimal C test framework for NIMCP simulation engines
 *
 * Usage:
 *   TEST(test_name) { ASSERT(condition); ASSERT_NEAR(a, b, tol); }
 *   int main() { RUN_TEST(test_name); PRINT_RESULTS(); return test_failures; }
 */

#ifndef NIMCP_TEST_FRAMEWORK_H
#define NIMCP_TEST_FRAMEWORK_H

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

static int test_pass_count = 0;
static int test_fail_count = 0;
static int test_total_asserts = 0;
static const char* current_test_name = "";

#define TEST(name) static void test_##name(void)

#define RUN_TEST(name) do { \
    current_test_name = #name; \
    test_##name(); \
    printf("  [PASS] %s\n", #name); \
    test_pass_count++; \
} while(0)

/* Wrap in a function-try pattern to catch assertion failures */
#define RUN_TEST_SAFE(name) do { \
    current_test_name = #name; \
    int _before_fails = test_fail_count; \
    test_##name(); \
    if (test_fail_count == _before_fails) { \
        printf("  [PASS] %s\n", #name); \
        test_pass_count++; \
    } \
} while(0)

#define ASSERT(cond) do { \
    test_total_asserts++; \
    if (!(cond)) { \
        printf("  [FAIL] %s: %s (line %d): %s\n", current_test_name, __FILE__, __LINE__, #cond); \
        test_fail_count++; \
        return; \
    } \
} while(0)

#define ASSERT_MSG(cond, msg) do { \
    test_total_asserts++; \
    if (!(cond)) { \
        printf("  [FAIL] %s (line %d): %s — %s\n", current_test_name, __LINE__, #cond, msg); \
        test_fail_count++; \
        return; \
    } \
} while(0)

#define ASSERT_NEAR(a, b, tol) do { \
    test_total_asserts++; \
    double _a = (double)(a), _b = (double)(b), _t = (double)(tol); \
    if (fabs(_a - _b) > _t) { \
        printf("  [FAIL] %s (line %d): |%.10g - %.10g| = %.10g > %.10g\n", \
               current_test_name, __LINE__, _a, _b, fabs(_a - _b), _t); \
        test_fail_count++; \
        return; \
    } \
} while(0)

#define ASSERT_NOT_NULL(ptr) do { \
    test_total_asserts++; \
    if ((ptr) == NULL) { \
        printf("  [FAIL] %s (line %d): %s is NULL\n", current_test_name, __LINE__, #ptr); \
        test_fail_count++; \
        return; \
    } \
} while(0)

#define ASSERT_NULL(ptr) ASSERT((ptr) == NULL)
#define ASSERT_TRUE(cond) ASSERT(cond)
#define ASSERT_FALSE(cond) ASSERT(!(cond))
#define ASSERT_EQ(a, b) ASSERT((a) == (b))
#define ASSERT_NE(a, b) ASSERT((a) != (b))
#define ASSERT_GT(a, b) ASSERT((a) > (b))
#define ASSERT_LT(a, b) ASSERT((a) < (b))
#define ASSERT_GE(a, b) ASSERT((a) >= (b))
#define ASSERT_LE(a, b) ASSERT((a) <= (b))

#define PRINT_RESULTS() do { \
    printf("\n========================================\n"); \
    printf("Results: %d passed, %d failed (%d assertions)\n", \
           test_pass_count, test_fail_count, test_total_asserts); \
    printf("========================================\n"); \
} while(0)

#define TEST_MAIN_BEGIN() int main(void) { printf("\n=== %s ===\n\n", __FILE__);
#define TEST_MAIN_END() PRINT_RESULTS(); return test_fail_count; }

#endif /* NIMCP_TEST_FRAMEWORK_H */
