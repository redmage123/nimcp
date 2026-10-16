/**
 * @file test_api_initialization.cpp
 * @brief GoogleTest unit tests for NIMCP API initialization and shutdown
 *
 * Tests the initialization lifecycle to ensure proper setup/cleanup
 * and idempotency guarantees.
 */

#include <gtest/gtest.h>
#include "nimcp.h"

/**
 * @brief Test fixture for API initialization tests
 */
class APIInitializationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Start each test with a clean shutdown state
        nimcp_shutdown();
    }

    void TearDown() override {
        // Always shutdown after each test
        nimcp_shutdown();
    }
};

/**
 * @brief Test that nimcp_init() succeeds
 */
TEST_F(APIInitializationTest, InitSucceeds) {
    nimcp_status_t status = nimcp_init();
    EXPECT_EQ(status, NIMCP_SUCCESS);
}

/**
 * @brief Test that nimcp_init() is idempotent (calling twice is safe)
 */
TEST_F(APIInitializationTest, InitIsIdempotent) {
    nimcp_status_t status1 = nimcp_init();
    EXPECT_EQ(status1, NIMCP_SUCCESS);

    // Second call should also succeed
    nimcp_status_t status2 = nimcp_init();
    EXPECT_EQ(status2, NIMCP_SUCCESS);

    // Third call for good measure
    nimcp_status_t status3 = nimcp_init();
    EXPECT_EQ(status3, NIMCP_SUCCESS);
}

/**
 * @brief Test that nimcp_shutdown() succeeds after init
 */
TEST_F(APIInitializationTest, ShutdownAfterInit) {
    nimcp_init();

    // Shutdown should not crash or fail
    EXPECT_NO_FATAL_FAILURE(nimcp_shutdown());
}

/**
 * @brief Test that nimcp_shutdown() when not initialized is safe
 */
TEST_F(APIInitializationTest, ShutdownWithoutInit) {
    // Shutdown without init should be safe (no crash)
    EXPECT_NO_FATAL_FAILURE(nimcp_shutdown());

    // Multiple shutdowns should also be safe
    EXPECT_NO_FATAL_FAILURE(nimcp_shutdown());
    EXPECT_NO_FATAL_FAILURE(nimcp_shutdown());
}

/**
 * @brief Test that error state is cleared after init
 */
TEST_F(APIInitializationTest, ErrorStateClearedAfterInit) {
    nimcp_status_t status = nimcp_init();
    EXPECT_EQ(status, NIMCP_SUCCESS);

    // Error should be "No error" after successful init
    const char* error = nimcp_get_error();
    EXPECT_STREQ(error, "No error");
}

/**
 * @brief Test init-shutdown-init cycle works correctly
 */
TEST_F(APIInitializationTest, InitShutdownInitCycle) {
    // First init
    nimcp_status_t status1 = nimcp_init();
    EXPECT_EQ(status1, NIMCP_SUCCESS);

    // Shutdown
    nimcp_shutdown();

    // Second init should succeed
    nimcp_status_t status2 = nimcp_init();
    EXPECT_EQ(status2, NIMCP_SUCCESS);
}

/**
 * @brief Test multiple init-shutdown cycles
 */
TEST_F(APIInitializationTest, MultipleInitShutdownCycles) {
    for (int i = 0; i < 5; i++) {
        nimcp_status_t status = nimcp_init();
        EXPECT_EQ(status, NIMCP_SUCCESS) << "Init failed on iteration " << i;

        nimcp_shutdown();
    }
}

/**
 * @brief Test that brain creation works after init
 */
TEST_F(APIInitializationTest, BrainCreationAfterInit) {
    nimcp_init();

    // Brain creation should work after init
    nimcp_brain_t brain = nimcp_brain_create(
        "test_brain",
        NIMCP_BRAIN_TINY,
        NIMCP_TASK_CLASSIFICATION,
        10,
        2
    );

    EXPECT_NE(brain, nullptr);

    if (brain) {
        nimcp_brain_destroy(brain);
    }
}

/**
 * @brief Test that brain creation without init still works
 *        (library should handle auto-init or graceful failure)
 */
TEST_F(APIInitializationTest, BrainCreationWithoutInit) {
    // Don't call nimcp_init()

    // Brain creation should either auto-init or handle gracefully
    nimcp_brain_t brain = nimcp_brain_create(
        "test_brain",
        NIMCP_BRAIN_TINY,
        NIMCP_TASK_CLASSIFICATION,
        10,
        2
    );

    // If auto-init happens, brain should be created
    // If not, brain should be NULL and error should be set
    if (brain) {
        nimcp_brain_destroy(brain);
    }

    // Either way, no crash should occur
    SUCCEED();
}

/**
 * @brief Test multiple shutdowns are safe
 */
TEST_F(APIInitializationTest, MultipleShutdownsSafe) {
    nimcp_init();

    // Multiple shutdowns should be safe
    nimcp_shutdown();
    nimcp_shutdown();
    nimcp_shutdown();

    SUCCEED();
}
