/**
 * @file test_multigpu_bounds.cpp
 * @brief Unit tests for multi-GPU device ID bounds validation
 *
 * WHAT: Test device ID bounds checking in multigpu_context_create
 * WHY:  P2-13 fix - accessing available_devices[device_id] without bounds check
 *       could cause stack buffer overflow / corruption
 * HOW:  Provide configs with out-of-range device IDs, verify graceful failure
 *
 * @author NIMCP Development Team
 * @date 2026-02-08
 */

#include <gtest/gtest.h>
#include "gpu/nimcp_multigpu.h"

//=============================================================================
// Test Fixtures
//=============================================================================

class MultiGPUBoundsTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Get the available device count for bounds testing
        multigpu_device_info_t devices[16];
        available_count = 0;
        multigpu_enumerate_devices(devices, 16, &available_count);
    }

    void TearDown() override {
        // Cleanup
    }

    uint32_t available_count;
};

//=============================================================================
// Bounds Validation Tests
//=============================================================================

/**
 * @brief P2-13: Config with device_id >= available count should fail gracefully
 *
 * WHAT: When device_ids array contains an ID beyond available_count,
 *       multigpu_context_create must reject it instead of accessing
 *       out-of-bounds stack memory
 * WHY:  Without bounds check, available_devices[device_id] causes stack
 *       buffer overflow when device_id >= 16 (stack array size)
 */
TEST_F(MultiGPUBoundsTest, MultiGPU_InvalidDeviceIDRejected) {
    // Skip if no devices available (can't test bounds)
    if (available_count == 0) {
        GTEST_SKIP() << "No GPU devices available for bounds testing";
    }

    // Create config with device ID that exceeds available count
    multigpu_config_t config = multigpu_default_config();
    int invalid_ids[] = { (int)available_count };  // One past the end
    config.num_devices = 1;
    config.device_ids = invalid_ids;

    multigpu_context_t ctx = multigpu_context_create(&config);

    // Must return NULL for out-of-bounds device ID
    EXPECT_EQ(ctx, nullptr)
        << "Context creation must fail when device_id >= available_count ("
        << available_count << ")";

    // Also test with a very large device ID
    int very_large_ids[] = { 9999 };
    config.device_ids = very_large_ids;

    ctx = multigpu_context_create(&config);
    EXPECT_EQ(ctx, nullptr)
        << "Context creation must fail when device_id is 9999";
}

/**
 * @brief P2-13: Config with negative device_id should fail gracefully
 *
 * WHAT: Negative device IDs must be rejected
 * WHY:  Negative index would access memory before the array
 */
TEST_F(MultiGPUBoundsTest, MultiGPU_NegativeDeviceIDRejected) {
    if (available_count == 0) {
        GTEST_SKIP() << "No GPU devices available for bounds testing";
    }

    multigpu_config_t config = multigpu_default_config();
    int negative_ids[] = { -1 };
    config.num_devices = 1;
    config.device_ids = negative_ids;

    multigpu_context_t ctx = multigpu_context_create(&config);

    EXPECT_EQ(ctx, nullptr)
        << "Context creation must fail when device_id is negative";

    // Test with INT_MIN
    int min_ids[] = { -2147483647 };
    config.device_ids = min_ids;

    ctx = multigpu_context_create(&config);
    EXPECT_EQ(ctx, nullptr)
        << "Context creation must fail when device_id is INT_MIN";
}

/**
 * @brief Valid device IDs should succeed
 *
 * WHAT: Device IDs within [0, available_count) should work
 * WHY:  Ensure bounds checking doesn't break valid use cases
 */
TEST_F(MultiGPUBoundsTest, MultiGPU_ValidDeviceIDAccepted) {
    if (available_count == 0) {
        GTEST_SKIP() << "No GPU devices available for bounds testing";
    }

    // Use device 0 which should always be valid
    multigpu_config_t config = multigpu_default_config();
    int valid_ids[] = { 0 };
    config.num_devices = 1;
    config.device_ids = valid_ids;

    multigpu_context_t ctx = multigpu_context_create(&config);

    // Must succeed with a valid device ID
    EXPECT_NE(ctx, nullptr)
        << "Context creation must succeed with device_id 0";

    if (ctx) {
        multigpu_context_destroy(ctx);
    }
}

/**
 * @brief Sequential device IDs (no device_ids array) should succeed
 *
 * WHAT: When config.device_ids is NULL, sequential IDs [0..N) are used
 * WHY:  Default behavior must work correctly within bounds
 */
TEST_F(MultiGPUBoundsTest, MultiGPU_NullDeviceIDsUsesSequential) {
    if (available_count == 0) {
        GTEST_SKIP() << "No GPU devices available for bounds testing";
    }

    multigpu_config_t config = multigpu_default_config();
    config.num_devices = 1;
    config.device_ids = NULL;  // Use sequential IDs

    multigpu_context_t ctx = multigpu_context_create(&config);

    // Must succeed with sequential IDs (device 0)
    EXPECT_NE(ctx, nullptr)
        << "Context creation must succeed with NULL device_ids (sequential)";

    if (ctx) {
        EXPECT_EQ(multigpu_get_device_count(ctx), 1u);
        multigpu_context_destroy(ctx);
    }
}
