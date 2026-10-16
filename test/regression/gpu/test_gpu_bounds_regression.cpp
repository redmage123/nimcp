/**
 * @file test_gpu_bounds_regression.cpp
 * @brief Regression test for GPU device ID bounds overflow
 *
 * WHAT: Regression test for P2-13 stack corruption via out-of-bounds device ID
 * WHY:  Prevent reintroduction of the bug where available_devices[device_id]
 *       accessed stack array without bounds validation
 * HOW:  Reproduce the exact scenario that triggered stack corruption:
 *       config with device_id >= available_count
 *
 * BUG DESCRIPTION:
 *   In multigpu_context_create(), available_devices is a stack-allocated
 *   array of 16 elements. When config->device_ids contained an ID >= the
 *   number of enumerated devices, the code accessed available_devices[device_id]
 *   without any bounds check, causing stack buffer overflow and potential
 *   code execution.
 *
 * @author NIMCP Development Team
 * @date 2026-02-08
 */

#include <gtest/gtest.h>
#include "gpu/nimcp_multigpu.h"

//=============================================================================
// Regression Test
//=============================================================================

/**
 * @brief Exact scenario that triggered stack corruption before P2-13 fix
 *
 * WHAT: Device ID set to available_count (one past end of array)
 * WHY:  This was the most common real-world trigger for the bug
 * HOW:  Create config with device_id = available_count, verify NULL return
 *
 * BEFORE FIX: This would access available_devices[available_count] which
 *             is one past the end of the stack array, corrupting stack
 *             variables and potentially hijacking control flow.
 *
 * AFTER FIX: multigpu_context_create returns NULL with proper error
 *            reporting via NIMCP_THROW_TO_IMMUNE.
 */
TEST(GPUBoundsRegression, Regression_DeviceIDBoundsOverflow) {
    // Step 1: Enumerate devices to get the count
    multigpu_device_info_t devices[16];
    uint32_t available_count = 0;
    bool enum_ok = multigpu_enumerate_devices(devices, 16, &available_count);

    // Must have at least one device for this test
    if (!enum_ok || available_count == 0) {
        GTEST_SKIP() << "No GPU devices available for regression test";
    }

    // Step 2: Create the exact scenario that triggered the bug
    // Device ID = available_count (one past the valid range)
    multigpu_config_t config = multigpu_default_config();
    int overflow_ids[] = { (int)available_count };
    config.num_devices = 1;
    config.device_ids = overflow_ids;

    // Step 3: This MUST NOT crash (it did before the fix)
    multigpu_context_t ctx = multigpu_context_create(&config);

    // Step 4: Verify proper rejection
    ASSERT_EQ(ctx, nullptr)
        << "REGRESSION: device_id=" << available_count
        << " (available=" << available_count << ") must be rejected. "
        << "If this passes but you see a crash, the bounds check is missing.";

    // Step 5: Test with device_id = 16 (stack array max size)
    // This is the maximum stack buffer overflow distance
    int max_stack_overflow_ids[] = { 16 };
    config.device_ids = max_stack_overflow_ids;

    ctx = multigpu_context_create(&config);
    ASSERT_EQ(ctx, nullptr)
        << "REGRESSION: device_id=16 (stack array size) must be rejected";

    // Step 6: Verify that valid IDs still work (no false positives)
    int valid_ids[] = { 0 };
    config.device_ids = valid_ids;

    ctx = multigpu_context_create(&config);
    EXPECT_NE(ctx, nullptr)
        << "Valid device_id=0 must still be accepted after bounds fix";

    if (ctx) {
        multigpu_context_destroy(ctx);
    }
}
