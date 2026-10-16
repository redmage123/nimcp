#ifndef NIMCP_TEST_BASE_H
#define NIMCP_TEST_BASE_H

#include <gtest/gtest.h>

extern "C" {
#include "utils/signal/nimcp_signal_handler.h"
#include "utils/memory/nimcp_memory.h"
#include "cognitive/consolidation/nimcp_consolidation.h"
}

/**
 * @file nimcp_test_base.h
 * @brief Base test class providing automatic global state cleanup for test isolation
 *
 * PROBLEM:
 * The NIMCP codebase contains 27+ global state variables that cause test flakiness.
 * Tests pass individually but fail when run in parallel due to state contamination
 * between tests. This includes:
 * - g_registered_brain: Global brain instance pointer
 * - Signal handler statistics and state
 * - Signal handler installation state
 * - And many more global variables across modules
 *
 * SOLUTION:
 * This base class automatically resets all known global state before and after each
 * test to ensure complete isolation. Tests inherit from NimcpTestBase instead of
 * ::testing::Test to get automatic cleanup.
 *
 * USAGE:
 * Instead of:
 *   class MyTest : public ::testing::Test { ... };
 *
 * Use:
 *   class MyTest : public NimcpTestBase { ... };
 *
 * The base class will automatically:
 * 1. Reset global state BEFORE each test (SetUp)
 * 2. Clean up global state AFTER each test (TearDown)
 *
 * ADDING NEW CLEANUP:
 * When you discover new global state that needs cleanup:
 * 1. Create a cleanup function in the appropriate module (e.g., my_module_reset_globals())
 * 2. Add the function call to both SetUp() and TearDown() below
 * 3. Document what global state it cleans up
 * 4. Consider making the cleanup function available from the module's public header
 *
 * EXAMPLE CLEANUP FUNCTION:
 * In your module's .c file:
 *   void my_module_reset_globals(void) {
 *       g_my_global_counter = 0;
 *       g_my_global_pointer = NULL;
 *       // Reset all module global state
 *   }
 *
 * In your module's .h file:
 *   void my_module_reset_globals(void);
 *
 * Then add to SetUp()/TearDown():
 *   my_module_reset_globals();
 */
class NimcpTestBase : public ::testing::Test {
protected:
    /**
     * @brief Set up test environment with clean global state
     *
     * Called automatically BEFORE each test. Resets all known global state to
     * ensure the test starts with a clean slate. This prevents state contamination
     * from previous tests.
     *
     * Global state cleaned:
     * - Signal handling state (g_registered_brain, handlers, stats)
     * - Memory tracker state (allocations, statistics, patterns)
     * - Consolidation statistics (g_sync_stats)
     * - Add more as discovered
     */
    void SetUp() override {
        // Signal handling cleanup
        // Clears g_registered_brain pointer to prevent use-after-free
        signal_handler_unregister_brain();

        // Resets signal statistics counters (SIGINT, SIGTERM, SIGUSR1 counts)
        signal_handler_reset_stats();

        // Uninstalls signal handlers to restore default behavior
        signal_handler_uninstall();

        // Memory tracker cleanup
        // Resets memory tracking state (allocations, statistics, patterns)
        nimcp_memory_reset_state();

        // Consolidation cleanup
        // Resets consolidation statistics (g_sync_stats)
        consolidation_reset_global_state();

        // TODO: Add cleanup calls for other modules here as they are discovered
        // Example:
        // deadlock_detector_reset();
        // dynamic_config_reset();
        // cache_reset_global_state();
    }

    /**
     * @brief Clean up test environment after test completion
     *
     * Called automatically AFTER each test. Performs the same cleanup as SetUp()
     * to ensure no state leaks to subsequent tests. This is defensive programming
     * in case tests don't clean up properly.
     *
     * Global state cleaned:
     * - Signal handling state (g_registered_brain, handlers, stats)
     * - Memory tracker state (allocations, statistics, patterns)
     * - Consolidation statistics (g_sync_stats)
     * - Add more as discovered
     */
    void TearDown() override {
        // Signal handling cleanup (same as SetUp for consistency)
        signal_handler_unregister_brain();
        signal_handler_reset_stats();
        signal_handler_uninstall();

        // Memory tracker cleanup (defensive cleanup)
        nimcp_memory_reset_state();

        // Consolidation cleanup (defensive cleanup)
        consolidation_reset_global_state();

        // TODO: Add cleanup calls for other modules here as they are discovered
        // Should mirror SetUp() cleanup calls
    }

public:
    /**
     * @brief Virtual destructor for proper cleanup in derived classes
     */
    virtual ~NimcpTestBase() = default;
};

/**
 * MIGRATION GUIDE:
 * ================
 *
 * To migrate existing tests to use NimcpTestBase:
 *
 * 1. Change the base class:
 *    OLD: class MyTest : public ::testing::Test
 *    NEW: class MyTest : public NimcpTestBase
 *
 * 2. Include this header:
 *    #include "test/utils/nimcp_test_base.h"
 *
 * 3. If your test has SetUp()/TearDown(), call parent methods:
 *    void SetUp() override {
 *        NimcpTestBase::SetUp();  // Call parent SetUp first
 *        // Your test-specific setup
 *    }
 *
 *    void TearDown() override {
 *        // Your test-specific cleanup
 *        NimcpTestBase::TearDown();  // Call parent TearDown last
 *    }
 *
 * 4. Remove any manual global state cleanup that's now handled by the base class
 *
 * BENEFITS:
 * =========
 * - Eliminates test flakiness from global state contamination
 * - Tests can run safely in parallel
 * - Reduces boilerplate cleanup code in individual tests
 * - Centralizes cleanup logic for easier maintenance
 * - Makes it obvious what global state exists in the system
 *
 * DEBUGGING:
 * ==========
 * If tests still fail in parallel after using NimcpTestBase:
 * 1. Check if there's global state not being cleaned up
 * 2. Add appropriate cleanup function to the module
 * 3. Update SetUp()/TearDown() in this file
 * 4. Document the cleanup in the comments above
 */

#endif // NIMCP_TEST_BASE_H
