/**
 * @file test_ema_isfinite_guards.c
 * @brief Unit tests for EMA isfinite() guards and safe math helpers
 * @date 2026-03-05
 *
 * WHAT: Tests that EMA calculations recover from NaN/Inf inputs
 * WHY:  A single NaN/Inf permanently corrupts an EMA without guards
 * HOW:  Feed NaN/Inf into EMA macros and helper functions, verify recovery
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <float.h>

#include "utils/math/nimcp_math_helpers.h"

/*=============================================================================
 * Test Framework
 *===========================================================================*/

static int g_tests_passed = 0;
static int g_tests_failed = 0;

#define TEST_ASSERT(cond, msg) do { \
    if (!(cond)) { \
        printf("  FAIL: %s (line %d)\n", msg, __LINE__); \
        g_tests_failed++; \
        return; \
    } \
} while(0)

#define TEST_PASS(msg) do { \
    printf("  PASS: %s\n", msg); \
    g_tests_passed++; \
} while(0)

/*=============================================================================
 * Test 1: NIMCP_EMA_GUARD resets NaN to fallback
 *===========================================================================*/
static void test_ema_guard_nan_reset(void)
{
    printf("Test 1: NIMCP_EMA_GUARD resets NaN to fallback\n");

    float ema = NAN;
    NIMCP_EMA_GUARD(ema, 42.0f);
    TEST_ASSERT(ema == 42.0f, "NaN should be reset to fallback value");

    TEST_PASS("EMA guard correctly resets NaN");
}

/*=============================================================================
 * Test 2: NIMCP_EMA_GUARD resets +Inf to fallback
 *===========================================================================*/
static void test_ema_guard_inf_reset(void)
{
    printf("Test 2: NIMCP_EMA_GUARD resets +Inf to fallback\n");

    float ema = INFINITY;
    NIMCP_EMA_GUARD(ema, 1.0f);
    TEST_ASSERT(ema == 1.0f, "+Inf should be reset to fallback value");

    ema = -INFINITY;
    NIMCP_EMA_GUARD(ema, -1.0f);
    TEST_ASSERT(ema == -1.0f, "-Inf should be reset to fallback value");

    TEST_PASS("EMA guard correctly resets Inf");
}

/*=============================================================================
 * Test 3: NIMCP_EMA_GUARD does not modify finite values
 *===========================================================================*/
static void test_ema_guard_finite_passthrough(void)
{
    printf("Test 3: NIMCP_EMA_GUARD does not modify finite values\n");

    float ema = 3.14f;
    NIMCP_EMA_GUARD(ema, 0.0f);
    TEST_ASSERT(ema == 3.14f, "Finite value should not be modified");

    ema = 0.0f;
    NIMCP_EMA_GUARD(ema, 99.0f);
    TEST_ASSERT(ema == 0.0f, "Zero should not be modified");

    ema = -100.0f;
    NIMCP_EMA_GUARD(ema, 0.0f);
    TEST_ASSERT(ema == -100.0f, "Negative finite value should not be modified");

    TEST_PASS("Finite values pass through unmodified");
}

/*=============================================================================
 * Test 4: NIMCP_EMA_GUARD_ZERO resets to zero
 *===========================================================================*/
static void test_ema_guard_zero(void)
{
    printf("Test 4: NIMCP_EMA_GUARD_ZERO resets NaN/Inf to zero\n");

    float ema = NAN;
    NIMCP_EMA_GUARD_ZERO(ema);
    TEST_ASSERT(ema == 0.0f, "NaN should reset to 0.0f");

    ema = INFINITY;
    NIMCP_EMA_GUARD_ZERO(ema);
    TEST_ASSERT(ema == 0.0f, "+Inf should reset to 0.0f");

    TEST_PASS("NIMCP_EMA_GUARD_ZERO works correctly");
}

/*=============================================================================
 * Test 5: nimcp_safe_expf guards against overflow
 *===========================================================================*/
static void test_safe_expf_overflow(void)
{
    printf("Test 5: nimcp_safe_expf guards against overflow\n");

    /* Normal values should work normally */
    float normal = nimcp_safe_expf(0.0f);
    TEST_ASSERT(fabsf(normal - 1.0f) < 1e-6f, "exp(0) should be 1.0");

    float e1 = nimcp_safe_expf(1.0f);
    TEST_ASSERT(fabsf(e1 - 2.71828f) < 0.001f, "exp(1) should be ~2.718");

    /* Large positive: should clamp, not overflow to Inf */
    float big = nimcp_safe_expf(100.0f);
    TEST_ASSERT(isfinite(big), "exp(100) should be clamped to finite");
    TEST_ASSERT(big > 0.0f, "exp(100) clamped result should be positive");

    float huge = nimcp_safe_expf(1000.0f);
    TEST_ASSERT(isfinite(huge), "exp(1000) should be clamped to finite");

    /* Large negative: should return near-zero (safe underflow) */
    float tiny = nimcp_safe_expf(-100.0f);
    TEST_ASSERT(isfinite(tiny), "exp(-100) should be finite");
    TEST_ASSERT(tiny >= 0.0f, "exp(-100) should be non-negative");

    float very_tiny = nimcp_safe_expf(-1000.0f);
    TEST_ASSERT(isfinite(very_tiny), "exp(-1000) should be finite");

    TEST_PASS("nimcp_safe_expf correctly handles extreme inputs");
}

/*=============================================================================
 * Test 6: EMA with NaN input does not propagate NaN
 *===========================================================================*/
static void test_ema_nan_recovery(void)
{
    printf("Test 6: EMA with NaN input does not propagate NaN\n");

    /* Simulate a running EMA that receives a NaN sample */
    float ema = 0.5f;
    float decay = 0.95f;

    /* Normal update */
    float value1 = 0.8f;
    ema = ema * decay + value1 * (1.0f - decay);
    TEST_ASSERT(isfinite(ema), "EMA after normal update should be finite");

    /* NaN update: without guard, this would permanently corrupt */
    float nan_value = NAN;
    ema = ema * decay + nan_value * (1.0f - decay);
    /* Now ema is NaN - the guard should fix it */
    NIMCP_EMA_GUARD(ema, value1);  /* Reset to last known good value */
    TEST_ASSERT(isfinite(ema), "EMA should be finite after NaN recovery");
    TEST_ASSERT(fabsf(ema - value1) < 1e-6f, "EMA should reset to fallback");

    /* Verify subsequent updates work normally */
    float value2 = 0.3f;
    ema = ema * decay + value2 * (1.0f - decay);
    NIMCP_EMA_GUARD(ema, value2);
    TEST_ASSERT(isfinite(ema), "EMA should remain finite after recovery");

    TEST_PASS("EMA recovers from NaN input");
}

/*=============================================================================
 * Test 7: EMA with Inf input does not propagate Inf
 *===========================================================================*/
static void test_ema_inf_recovery(void)
{
    printf("Test 7: EMA with Inf input does not propagate Inf\n");

    float ema = 1.0f;
    float decay = 0.99f;

    /* Feed +Inf */
    float inf_value = INFINITY;
    ema = ema * decay + inf_value * (1.0f - decay);
    NIMCP_EMA_GUARD(ema, 1.0f);
    TEST_ASSERT(isfinite(ema), "EMA should be finite after +Inf recovery");

    /* Feed -Inf */
    ema = 0.5f;
    float neg_inf = -INFINITY;
    ema = ema * decay + neg_inf * (1.0f - decay);
    NIMCP_EMA_GUARD(ema, 0.5f);
    TEST_ASSERT(isfinite(ema), "EMA should be finite after -Inf recovery");

    TEST_PASS("EMA recovers from Inf input");
}

/*=============================================================================
 * Test 8: Exponential decay with extreme dt does not corrupt
 *===========================================================================*/
static void test_exp_decay_extreme_dt(void)
{
    printf("Test 8: Exponential decay with extreme dt\n");

    /* Simulate conductance *= exp(-dt / tau) with extreme dt */
    float conductance = 1.0f;
    float tau = 10.0f;

    /* Normal decay */
    float dt_normal = 1.0f;
    conductance *= nimcp_safe_expf(-dt_normal / tau);
    NIMCP_EMA_GUARD_ZERO(conductance);
    TEST_ASSERT(isfinite(conductance), "Normal decay should be finite");
    TEST_ASSERT(conductance > 0.0f && conductance < 1.0f, "Conductance should decrease");

    /* Very large dt (should decay to ~0, not produce NaN) */
    conductance = 1.0f;
    float dt_huge = 1e10f;
    conductance *= nimcp_safe_expf(-dt_huge / tau);
    NIMCP_EMA_GUARD_ZERO(conductance);
    TEST_ASSERT(isfinite(conductance), "Huge dt decay should be finite");
    TEST_ASSERT(conductance >= 0.0f, "Conductance after huge dt should be non-negative");

    /* Very small tau (large exponent magnitude) */
    conductance = 1.0f;
    float tau_tiny = 1e-10f;
    float dt_small = 1.0f;
    conductance *= nimcp_safe_expf(-dt_small / tau_tiny);
    NIMCP_EMA_GUARD_ZERO(conductance);
    TEST_ASSERT(isfinite(conductance), "Tiny tau decay should be finite");

    /* Negative dt (should not happen but guard against it) */
    conductance = 1.0f;
    float dt_negative = -1000.0f;
    conductance *= nimcp_safe_expf(-dt_negative / tau);
    NIMCP_EMA_GUARD_ZERO(conductance);
    TEST_ASSERT(isfinite(conductance), "Negative dt decay should be finite");

    TEST_PASS("Exponential decay handles extreme dt values");
}

/*=============================================================================
 * Test 9: Chained EMA updates with intermittent NaN
 *===========================================================================*/
static void test_chained_ema_nan_intermittent(void)
{
    printf("Test 9: Chained EMA updates with intermittent NaN\n");

    float ema = 0.0f;
    float decay = 0.95f;

    /* Run 100 updates, injecting NaN every 10th iteration */
    for (int i = 0; i < 100; i++) {
        float value;
        if (i % 10 == 5) {
            value = NAN;  /* NaN every 10th iteration */
        } else {
            value = (float)i * 0.01f;
        }

        ema = ema * decay + value * (1.0f - decay);
        NIMCP_EMA_GUARD(ema, (float)i * 0.01f);  /* Fallback to expected value */

        TEST_ASSERT(isfinite(ema),
            "EMA must remain finite throughout 100 iterations with NaN injection");
    }

    /* Final value should be reasonable (close to 0.99 * 0.01 = ~0.99 smoothed) */
    TEST_ASSERT(ema > 0.0f && ema < 2.0f, "Final EMA should be in reasonable range");

    TEST_PASS("Chained EMA survives intermittent NaN injection");
}

/*=============================================================================
 * Test 10: nimcp_safe_expf with NaN input
 *===========================================================================*/
static void test_safe_expf_nan_input(void)
{
    printf("Test 10: nimcp_safe_expf with NaN input\n");

    /* NaN input to regular expf produces NaN */
    float regular = expf(NAN);
    TEST_ASSERT(!isfinite(regular) || regular != regular,
        "Regular expf(NaN) should produce NaN");

    /* The clamping in nimcp_safe_expf doesn't help with NaN since
       NaN comparisons are always false. This tests the limitation. */
    /* For robustness, we verify that even if safe_expf receives NaN,
       the EMA guard catches it downstream */
    float conductance = 1.0f;
    conductance *= expf(NAN);  /* This produces NaN */
    NIMCP_EMA_GUARD_ZERO(conductance);
    TEST_ASSERT(conductance == 0.0f, "Guard should catch NaN from expf(NaN)");

    TEST_PASS("NaN from expf is caught by downstream guard");
}

/*=============================================================================
 * Main
 *===========================================================================*/
int main(void)
{
    printf("=== EMA isfinite() Guard Tests ===\n\n");

    test_ema_guard_nan_reset();
    test_ema_guard_inf_reset();
    test_ema_guard_finite_passthrough();
    test_ema_guard_zero();
    test_safe_expf_overflow();
    test_ema_nan_recovery();
    test_ema_inf_recovery();
    test_exp_decay_extreme_dt();
    test_chained_ema_nan_intermittent();
    test_safe_expf_nan_input();

    printf("\n=== Results: %d passed, %d failed ===\n",
           g_tests_passed, g_tests_failed);

    return g_tests_failed > 0 ? 1 : 0;
}
