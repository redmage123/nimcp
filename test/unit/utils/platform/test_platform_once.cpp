/**
 * @file test_platform_once.cpp
 * @brief Comprehensive TDD test suite for platform once-initialization module
 *
 * WHAT: Test suite for cross-platform once-initialization abstraction
 * WHY:  Ensure once semantics are correct across POSIX and Windows platforms
 * HOW:  Use GoogleTest with multi-threaded scenarios and state verification
 *
 * TEST COVERAGE:
 * 1. Basic once initialization - function runs exactly once
 * 2. Multiple calls - subsequent calls don't re-run function
 * 3. Concurrent calls - multiple threads, runs once total
 * 4. Different once controls - independent controls are separate
 * 5. NULL pointer safety - graceful handling of invalid inputs
 *
 * @author NIMCP Development Team
 * @date 2025
 */

#include "test_helpers.h"
#include <thread>
#include <vector>
#include <atomic>
#include <mutex>
#include <memory>

#include "utils/platform/nimcp_platform_once.h"

//=============================================================================
// Test Constants and Global State
//=============================================================================

/**
 * Counter to track initialization routine calls.
 * Used to verify that a function is called exactly once.
 */
static std::atomic<int> g_init_call_count(0);

/**
 * Thread counter to track how many threads entered the initialization.
 * Used to verify that only one thread executes the routine.
 */
static std::atomic<int> g_thread_count(0);

/**
 * Initialization barrier to ensure all threads are ready.
 */
static std::atomic<bool> g_barrier_ready(false);

/**
 * Test value that gets modified during initialization.
 */
static int g_test_value = 0;

/**
 * Mutex to protect access to g_test_value.
 */
static std::mutex g_test_mutex;

//=============================================================================
// Test Helper Functions
//=============================================================================

/**
 * WHAT: Simple initialization routine that increments counter
 * WHY:  Verify the routine is called exactly once
 * HOW:  Increment atomic counter in a thread-safe manner
 */
static void simple_init_routine(void)
{
    g_init_call_count++;
}

/**
 * WHAT: Initialization routine that modifies test value
 * WHY:  Verify state changes from once routine are visible
 * HOW:  Lock mutex and modify global test value
 */
static void test_value_init_routine(void)
{
    std::lock_guard<std::mutex> lock(g_test_mutex);
    g_test_value = 42;
}

/**
 * WHAT: Initialization routine that tracks thread count
 * WHY:  Verify only one thread executes the routine
 * HOW:  Increment thread counter in once routine
 */
static void thread_tracking_init_routine(void)
{
    g_thread_count++;
}

/**
 * WHAT: Initialization routine that does work (simulate resource allocation)
 * WHY:  Verify once semantics with realistic workload
 * HOW:  Allocate memory or perform initialization work
 */
static void resource_allocation_routine(void)
{
    std::lock_guard<std::mutex> lock(g_test_mutex);
    // Simulate some work
    for (int i = 0; i < 1000; i++) {
        g_test_value = g_test_value * 2 + 1;
        if (g_test_value > 1000000) {
            g_test_value = i;
        }
    }
}

/**
 * WHAT: Reset global test state between tests
 * WHY:  Ensure tests don't interfere with each other
 * HOW:  Reset all atomic counters and values
 */
static void reset_test_state(void)
{
    g_init_call_count = 0;
    g_thread_count = 0;
    g_barrier_ready = false;
    g_test_value = 0;
}

//=============================================================================
// Basic Once Initialization Tests
//=============================================================================

/**
 * TEST: BasicOnce
 * WHAT: Verify that a function runs exactly once
 * WHY:  Ensure fundamental once semantics work
 * HOW:  Call once routine and verify counter is 1
 *
 * COVERAGE:
 * - Valid once control initialization
 * - Function execution on first call
 * - Counter verification
 */
TEST(PlatformOnce, BasicOnce)
{
    reset_test_state();

    static nimcp_platform_once_t once_control = NIMCP_PLATFORM_ONCE_INIT;

    // First call should execute the routine
    int result = nimcp_platform_once(&once_control, simple_init_routine);

    ASSERT_EQ(result, 0);
    ASSERT_EQ(g_init_call_count, 1);
}

/**
 * TEST: BasicOnceWithValue
 * WHAT: Verify that state changes from once routine are visible
 * WHY:  Ensure initialization side effects persist
 * HOW:  Call once routine that modifies global value and verify
 *
 * COVERAGE:
 * - State modification in once routine
 * - Visibility of changes after once returns
 * - Memory consistency
 */
TEST(PlatformOnce, BasicOnceWithValue)
{
    reset_test_state();

    static nimcp_platform_once_t once_control = NIMCP_PLATFORM_ONCE_INIT;

    // Call the once routine
    int result = nimcp_platform_once(&once_control, test_value_init_routine);

    ASSERT_EQ(result, 0);
    ASSERT_EQ(g_test_value, 42);
}

//=============================================================================
// Multiple Calls Tests
//=============================================================================

/**
 * TEST: MultipleCalls
 * WHAT: Verify that subsequent calls don't re-run function
 * WHY:  Ensure once semantics - function runs only once
 * HOW:  Call once routine multiple times and verify counter stays at 1
 *
 * COVERAGE:
 * - First call executes routine
 * - Subsequent calls skip execution
 * - Return value consistency
 * - Counter remains unchanged
 */
TEST(PlatformOnce, MultipleCalls)
{
    reset_test_state();

    static nimcp_platform_once_t once_control = NIMCP_PLATFORM_ONCE_INIT;

    // First call
    int result1 = nimcp_platform_once(&once_control, simple_init_routine);
    ASSERT_EQ(result1, 0);
    ASSERT_EQ(g_init_call_count, 1);

    // Second call
    int result2 = nimcp_platform_once(&once_control, simple_init_routine);
    ASSERT_EQ(result2, 0);
    ASSERT_EQ(g_init_call_count, 1);  // Still 1!

    // Third call
    int result3 = nimcp_platform_once(&once_control, simple_init_routine);
    ASSERT_EQ(result3, 0);
    ASSERT_EQ(g_init_call_count, 1);  // Still 1!
}

/**
 * TEST: MultipleCallsSequential
 * WHAT: Verify multiple sequential calls work correctly
 * WHY:  Ensure once control remains valid after first execution
 * HOW:  Call same once routine 100 times in sequence
 *
 * COVERAGE:
 * - Long sequence of calls
 * - State stability
 * - Return code consistency
 */
TEST(PlatformOnce, MultipleCallsSequential)
{
    reset_test_state();

    static nimcp_platform_once_t once_control = NIMCP_PLATFORM_ONCE_INIT;

    for (int i = 0; i < 100; i++) {
        int result = nimcp_platform_once(&once_control, simple_init_routine);
        ASSERT_EQ(result, 0);
        ASSERT_EQ(g_init_call_count, 1);
    }
}

/**
 * TEST: MultipleValuesCall
 * WHAT: Verify state changes persist across multiple calls
 * WHY:  Ensure side effects from initialization are lasting
 * HOW:  Call once, then multiple times, verify value remains
 *
 * COVERAGE:
 * - State persistence
 * - Memory consistency across calls
 * - Value integrity
 */
TEST(PlatformOnce, MultipleValuesCall)
{
    reset_test_state();

    static nimcp_platform_once_t once_control = NIMCP_PLATFORM_ONCE_INIT;

    // First call modifies state
    nimcp_platform_once(&once_control, test_value_init_routine);
    ASSERT_EQ(g_test_value, 42);

    // Subsequent calls don't modify state, value should persist
    for (int i = 0; i < 50; i++) {
        nimcp_platform_once(&once_control, test_value_init_routine);
        ASSERT_EQ(g_test_value, 42);
    }
}

//=============================================================================
// Concurrent Calls Tests
//=============================================================================

/**
 * TEST: ConcurrentOnce
 * WHAT: Verify multiple threads, runs once total
 * WHY:  Ensure thread-safety of once semantics
 * HOW:  Spawn multiple threads calling once simultaneously
 *
 * COVERAGE:
 * - Thread safety
 * - Synchronization
 * - Once semantics with concurrency
 * - Race condition prevention
 *
 * EXPECTED BEHAVIOR:
 * - Only one thread executes routine
 * - All threads return success (0)
 * - Counter equals 1 after all threads complete
 */
TEST(PlatformOnce, ConcurrentOnce)
{
    reset_test_state();

    static nimcp_platform_once_t once_control = NIMCP_PLATFORM_ONCE_INIT;
    const int num_threads = 10;
    std::vector<std::thread> threads;

    // Spawn threads that all call once
    for (int i = 0; i < num_threads; i++) {
        threads.emplace_back([&]() {
            int result = nimcp_platform_once(&once_control, simple_init_routine);
            ASSERT_EQ(result, 0);
        });
    }

    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }

    // Only one call should have executed
    ASSERT_EQ(g_init_call_count, 1);
}

/**
 * TEST: HighConcurrencyCalls
 * WHAT: Verify once semantics under high concurrency (50 threads)
 * WHY:  Ensure robustness with many competing threads
 * HOW:  Spawn 50 threads calling once simultaneously
 *
 * COVERAGE:
 * - Stress testing
 * - Race condition prevention
 * - Performance with high concurrency
 */
TEST(PlatformOnce, HighConcurrencyCalls)
{
    reset_test_state();

    static nimcp_platform_once_t once_control = NIMCP_PLATFORM_ONCE_INIT;
    const int num_threads = 50;
    std::vector<std::thread> threads;

    for (int i = 0; i < num_threads; i++) {
        threads.emplace_back([&]() {
            nimcp_platform_once(&once_control, simple_init_routine);
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    ASSERT_EQ(g_init_call_count, 1);
}

/**
 * TEST: ConcurrentStateModification
 * WHAT: Verify state modifications are visible across all threads
 * WHY:  Ensure memory synchronization with concurrent once calls
 * HOW:  Threads call once that modifies state, verify visibility
 *
 * COVERAGE:
 * - Memory consistency across threads
 * - State visibility
 * - Synchronization barriers
 */
TEST(PlatformOnce, ConcurrentStateModification)
{
    reset_test_state();

    static nimcp_platform_once_t once_control = NIMCP_PLATFORM_ONCE_INIT;
    const int num_threads = 20;
    std::vector<std::thread> threads;
    std::vector<int> thread_values(num_threads);

    for (int i = 0; i < num_threads; i++) {
        threads.emplace_back([i, &thread_values]() {
            nimcp_platform_once(&once_control, test_value_init_routine);
            // After once returns, state should be initialized
            std::lock_guard<std::mutex> lock(g_test_mutex);
            thread_values[i] = g_test_value;
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    // All threads should see the same value
    for (int i = 0; i < num_threads; i++) {
        ASSERT_EQ(thread_values[i], 42);
    }
}

/**
 * TEST: ConcurrentRepeatedCalls
 * WHAT: Verify repeated concurrent calls all return success
 * WHY:  Ensure once remains correct with repeated concurrent access
 * HOW:  Multiple threads call once multiple times
 *
 * COVERAGE:
 * - Concurrent repeated calls
 * - Consistency under repeated access
 * - Performance characteristics
 */
TEST(PlatformOnce, ConcurrentRepeatedCalls)
{
    reset_test_state();

    static nimcp_platform_once_t once_control = NIMCP_PLATFORM_ONCE_INIT;
    const int num_threads = 10;
    const int calls_per_thread = 10;
    std::vector<std::thread> threads;
    std::atomic<int> success_count(0);

    for (int i = 0; i < num_threads; i++) {
        threads.emplace_back([&success_count]() {
            for (int j = 0; j < calls_per_thread; j++) {
                int result = nimcp_platform_once(&once_control, simple_init_routine);
                if (result == 0) {
                    success_count++;
                }
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    // All calls should succeed
    ASSERT_EQ(success_count, num_threads * calls_per_thread);
    // Only one actual initialization
    ASSERT_EQ(g_init_call_count, 1);
}

//=============================================================================
// Multiple Once Controls Tests
//=============================================================================

/**
 * TEST: MultipleOnceControls
 * WHAT: Verify different controls are independent
 * WHY:  Ensure once controls don't interfere with each other
 * HOW:  Create multiple once controls and verify each runs once independently
 *
 * COVERAGE:
 * - Multiple independent once controls
 * - Isolation between controls
 * - Per-control state tracking
 */
TEST(PlatformOnce, MultipleOnceControls)
{
    reset_test_state();

    static nimcp_platform_once_t once_control1 = NIMCP_PLATFORM_ONCE_INIT;
    static nimcp_platform_once_t once_control2 = NIMCP_PLATFORM_ONCE_INIT;

    // Call first control
    int result1 = nimcp_platform_once(&once_control1, simple_init_routine);
    ASSERT_EQ(result1, 0);
    ASSERT_EQ(g_init_call_count, 1);

    // Call second control - should execute its own routine
    int result2 = nimcp_platform_once(&once_control2, simple_init_routine);
    ASSERT_EQ(result2, 0);
    ASSERT_EQ(g_init_call_count, 2);

    // Calling first control again should not re-execute
    result1 = nimcp_platform_once(&once_control1, simple_init_routine);
    ASSERT_EQ(result1, 0);
    ASSERT_EQ(g_init_call_count, 2);

    // Calling second control again should not re-execute
    result2 = nimcp_platform_once(&once_control2, simple_init_routine);
    ASSERT_EQ(result2, 0);
    ASSERT_EQ(g_init_call_count, 2);
}

/**
 * TEST: ThreeIndependentControls
 * WHAT: Verify three independent once controls work correctly
 * WHY:  Ensure scalability with multiple independent controls
 * HOW:  Create and use three separate once controls
 *
 * COVERAGE:
 * - Multiple independent once controls
 * - Isolation verification
 * - Scalability testing
 */
TEST(PlatformOnce, ThreeIndependentControls)
{
    reset_test_state();

    static nimcp_platform_once_t once_ctrl1 = NIMCP_PLATFORM_ONCE_INIT;
    static nimcp_platform_once_t once_ctrl2 = NIMCP_PLATFORM_ONCE_INIT;
    static nimcp_platform_once_t once_ctrl3 = NIMCP_PLATFORM_ONCE_INIT;

    // Each control should run its own routine once
    for (int i = 0; i < 5; i++) {
        nimcp_platform_once(&once_ctrl1, simple_init_routine);
        nimcp_platform_once(&once_ctrl2, simple_init_routine);
        nimcp_platform_once(&once_ctrl3, simple_init_routine);
    }

    // Should have been called exactly 3 times (one per control)
    ASSERT_EQ(g_init_call_count, 3);
}

/**
 * TEST: MultipleControlsConcurrent
 * WHAT: Verify multiple controls with concurrent access
 * WHY:  Ensure controls remain independent under concurrency
 * HOW:  Multiple threads call different once controls
 *
 * COVERAGE:
 * - Multiple controls with concurrency
 * - Thread safety per control
 * - Independence verification
 */
TEST(PlatformOnce, MultipleControlsConcurrent)
{
    reset_test_state();

    static nimcp_platform_once_t once_ctrl1 = NIMCP_PLATFORM_ONCE_INIT;
    static nimcp_platform_once_t once_ctrl2 = NIMCP_PLATFORM_ONCE_INIT;

    const int num_threads = 20;
    std::vector<std::thread> threads;

    for (int i = 0; i < num_threads; i++) {
        if (i % 2 == 0) {
            threads.emplace_back([&]() {
                nimcp_platform_once(&once_ctrl1, simple_init_routine);
            });
        } else {
            threads.emplace_back([&]() {
                nimcp_platform_once(&once_ctrl2, simple_init_routine);
            });
        }
    }

    for (auto& thread : threads) {
        thread.join();
    }

    // Each control should have been called once
    ASSERT_EQ(g_init_call_count, 2);
}

//=============================================================================
// NULL Pointer Safety Tests
//=============================================================================

/**
 * TEST: NullOnceControl
 * WHAT: Verify NULL once control is handled gracefully
 * WHY:  Ensure robustness against invalid inputs
 * HOW:  Call once with NULL once_control and expect error
 *
 * COVERAGE:
 * - NULL pointer validation
 * - Error code return (EINVAL)
 * - No crash or undefined behavior
 */
TEST(PlatformOnce, NullOnceControl)
{
    reset_test_state();

    // Call with NULL once_control
    int result = nimcp_platform_once(nullptr, simple_init_routine);

    // Should return error
    ASSERT_NE(result, 0);
    // Routine should not have been called
    ASSERT_EQ(g_init_call_count, 0);
}

/**
 * TEST: NullInitRoutine
 * WHAT: Verify NULL init routine is handled gracefully
 * WHY:  Ensure robustness against invalid callbacks
 * HOW:  Call once with NULL init_routine and expect error
 *
 * COVERAGE:
 * - NULL callback validation
 * - Error code return
 * - Prevention of null pointer dereference
 */
TEST(PlatformOnce, NullInitRoutine)
{
    reset_test_state();

    static nimcp_platform_once_t once_control = NIMCP_PLATFORM_ONCE_INIT;

    // Call with NULL init_routine
    int result = nimcp_platform_once(&once_control, nullptr);

    // Should return error
    ASSERT_NE(result, 0);
    // Counter should remain 0
    ASSERT_EQ(g_init_call_count, 0);
}

/**
 * TEST: BothArgumentsNull
 * WHAT: Verify both arguments NULL is handled safely
 * WHY:  Ensure validation catches invalid parameter combinations
 * HOW:  Call once with both NULL parameters
 *
 * COVERAGE:
 * - Combined parameter validation
 * - Error handling
 * - Safety against multiple NULL values
 */
TEST(PlatformOnce, BothArgumentsNull)
{
    reset_test_state();

    // Call with both NULL
    int result = nimcp_platform_once(nullptr, nullptr);

    // Should return error
    ASSERT_NE(result, 0);
    // Counter should remain 0
    ASSERT_EQ(g_init_call_count, 0);
}

/**
 * TEST: NullSafetyMultipleThreads
 * WHAT: Verify NULL safety under multi-threaded access
 * WHY:  Ensure no race conditions with NULL pointer checks
 * HOW:  Multiple threads call once with NULL parameters
 *
 * COVERAGE:
 * - Concurrent NULL pointer handling
 * - Thread-safe validation
 * - Race condition prevention
 */
TEST(PlatformOnce, NullSafetyMultipleThreads)
{
    reset_test_state();

    const int num_threads = 10;
    std::vector<std::thread> threads;

    for (int i = 0; i < num_threads; i++) {
        threads.emplace_back([&]() {
            // Some threads call with NULL once_control
            int result = nimcp_platform_once(nullptr, simple_init_routine);
            ASSERT_NE(result, 0);
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    // No routines should have been called
    ASSERT_EQ(g_init_call_count, 0);
}

//=============================================================================
// Edge Case and Stress Tests
//=============================================================================

/**
 * TEST: VeryLongSequential
 * WHAT: Verify once works correctly with very long sequences
 * WHY:  Ensure stability under sustained access patterns
 * HOW:  Call once 1000 times in sequence
 *
 * COVERAGE:
 * - Long-term stability
 * - Performance consistency
 * - Counter overflow prevention
 */
TEST(PlatformOnce, VeryLongSequential)
{
    reset_test_state();

    static nimcp_platform_once_t once_control = NIMCP_PLATFORM_ONCE_INIT;

    for (int i = 0; i < 1000; i++) {
        int result = nimcp_platform_once(&once_control, simple_init_routine);
        ASSERT_EQ(result, 0);
    }

    ASSERT_EQ(g_init_call_count, 1);
}

/**
 * TEST: HeavyWorkloadInit
 * WHAT: Verify once works with computationally expensive routine
 * WHY:  Ensure once semantics hold with real work
 * HOW:  Call once with routine that does substantial work
 *
 * COVERAGE:
 * - Real-world initialization patterns
 * - Performance with actual work
 * - State consistency with complex initialization
 */
TEST(PlatformOnce, HeavyWorkloadInit)
{
    reset_test_state();

    static nimcp_platform_once_t once_control = NIMCP_PLATFORM_ONCE_INIT;

    // Call with heavy routine
    int result = nimcp_platform_once(&once_control, resource_allocation_routine);
    ASSERT_EQ(result, 0);

    int initial_value = g_test_value;

    // Subsequent calls should not re-execute
    for (int i = 0; i < 10; i++) {
        nimcp_platform_once(&once_control, resource_allocation_routine);
        std::lock_guard<std::mutex> lock(g_test_mutex);
        ASSERT_EQ(g_test_value, initial_value);
    }
}

/**
 * TEST: ConcurrentHeavyWorkload
 * WHAT: Verify concurrent access with heavy initialization work
 * WHY:  Ensure thread safety with realistic workloads
 * HOW:  Multiple threads call once with heavy routine
 *
 * COVERAGE:
 * - Concurrent heavy workloads
 * - Synchronization with complex initialization
 * - Performance under load
 */
TEST(PlatformOnce, ConcurrentHeavyWorkload)
{
    reset_test_state();

    static nimcp_platform_once_t once_control = NIMCP_PLATFORM_ONCE_INIT;
    const int num_threads = 15;
    std::vector<std::thread> threads;

    for (int i = 0; i < num_threads; i++) {
        threads.emplace_back([&]() {
            nimcp_platform_once(&once_control, resource_allocation_routine);
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    // Only one thread should have executed the heavy routine
    // (We can't directly verify this due to the nature of the routine,
    // but we ensure no crashes and state is consistent)
    EXPECT_GT(g_test_value, 0);
}

//=============================================================================
// Return Value Tests
//=============================================================================

/**
 * TEST: ReturnValueSuccess
 * WHAT: Verify successful once calls return 0
 * WHY:  Ensure standard error code convention
 * HOW:  Call once and verify return value is 0
 *
 * COVERAGE:
 * - Return value conventions
 * - Error code consistency
 */
TEST(PlatformOnce, ReturnValueSuccess)
{
    reset_test_state();

    static nimcp_platform_once_t once_control = NIMCP_PLATFORM_ONCE_INIT;

    int result = nimcp_platform_once(&once_control, simple_init_routine);
    ASSERT_EQ(result, 0);
}

/**
 * TEST: ReturnValueMultipleCalls
 * WHAT: Verify all calls (first and subsequent) return 0
 * WHY:  Ensure consistent success reporting
 * HOW:  Call multiple times and verify all return 0
 *
 * COVERAGE:
 * - Return value consistency across calls
 * - Success indication on all calls
 */
TEST(PlatformOnce, ReturnValueMultipleCalls)
{
    reset_test_state();

    static nimcp_platform_once_t once_control = NIMCP_PLATFORM_ONCE_INIT;

    for (int i = 0; i < 100; i++) {
        int result = nimcp_platform_once(&once_control, simple_init_routine);
        ASSERT_EQ(result, 0);
    }
}

//=============================================================================
// Initialization Order Tests
//=============================================================================

/**
 * TEST: OrderedInitialization
 * WHAT: Verify that initialization happens before return
 * WHY:  Ensure state is ready when once returns
 * HOW:  Check state immediately after once returns
 *
 * COVERAGE:
 * - Initialization timing
 * - State readiness guarantee
 * - Return synchronization
 */
TEST(PlatformOnce, OrderedInitialization)
{
    reset_test_state();

    static nimcp_platform_once_t once_control = NIMCP_PLATFORM_ONCE_INIT;

    // Call once
    nimcp_platform_once(&once_control, test_value_init_routine);

    // State should be ready immediately
    ASSERT_EQ(g_test_value, 42);
}

/**
 * TEST: OrderedInitializationConcurrent
 * WHAT: Verify all threads see initialized state after once
 * WHY:  Ensure happens-before relationship with initialization
 * HOW:  Threads wait for once to return, check state
 *
 * COVERAGE:
 * - Concurrent visibility of initialization
 * - Memory ordering guarantees
 * - State synchronization
 */
TEST(PlatformOnce, OrderedInitializationConcurrent)
{
    reset_test_state();

    static nimcp_platform_once_t once_control = NIMCP_PLATFORM_ONCE_INIT;
    const int num_threads = 20;
    std::vector<std::thread> threads;
    std::vector<int> observed_values(num_threads, -1);

    for (int i = 0; i < num_threads; i++) {
        threads.emplace_back([i, &observed_values]() {
            nimcp_platform_once(&once_control, test_value_init_routine);
            // After once returns, state must be initialized
            std::lock_guard<std::mutex> lock(g_test_mutex);
            observed_values[i] = g_test_value;
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    // All threads should see the same initialized value
    for (int i = 0; i < num_threads; i++) {
        ASSERT_EQ(observed_values[i], 42);
    }
}

//=============================================================================
// Test Teardown
//=============================================================================

/**
 * Global cleanup after all tests in this suite
 */
class PlatformOnceTestSuite : public ::testing::Test {
   protected:
    void TearDown() override
    {
        reset_test_state();
    }
};
