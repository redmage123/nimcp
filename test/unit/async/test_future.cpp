/**
 * @file test_future.cpp
 * @brief Comprehensive unit tests for NIMCP futures/promises system
 *
 * WHAT: Complete test coverage for async futures/promises API
 * WHY:  Ensure all async operations work correctly across threads
 * HOW:  GTest framework with fixtures, thread safety tests, combinators
 *
 * TEST COVERAGE:
 * - Basic creation and destruction
 * - State transitions (PENDING → COMPLETED/FAILED/CANCELLED)
 * - Promise value/error setting
 * - Future waiting (blocking, timeout, non-blocking)
 * - Cancellation
 * - Callbacks and chaining
 * - Combinators (all, any, map)
 * - Thread safety
 * - Memory management
 * - Error propagation
 *
 * @author NIMCP Development Team
 * @date 2025-11-28
 */

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include "async/nimcp_future.h"
#include "utils/error/nimcp_error_codes.h"

//=============================================================================
// Test Fixture
//=============================================================================

/**
 * @brief Test fixture for futures/promises tests
 *
 * WHAT: Provides setup/teardown and helper methods
 * WHY:  Ensure clean state for each test, prevent leaks
 * HOW:  Tracks created futures/promises, cleans up in TearDown
 */
class FutureTest : public ::testing::Test {
protected:
    std::vector<nimcp_promise_t> promises;
    std::vector<nimcp_future_t> futures;

    void SetUp() override {
        // Reset statistics for clean measurements
        nimcp_future_reset_stats();
    }

    void TearDown() override {
        // Clean up all created futures
        for (auto future : futures) {
            if (future) {
                nimcp_future_destroy(future);
            }
        }
        futures.clear();

        // Clean up all created promises
        for (auto promise : promises) {
            if (promise) {
                nimcp_promise_destroy(promise);
            }
        }
        promises.clear();
    }

    // Helper: Create promise and track it
    nimcp_promise_t create_promise(size_t result_size) {
        nimcp_promise_t promise = nimcp_promise_create(result_size);
        if (promise) {
            promises.push_back(promise);
        }
        return promise;
    }

    // Helper: Get future and track it
    nimcp_future_t get_future(nimcp_promise_t promise) {
        nimcp_future_t future = nimcp_promise_get_future(promise);
        if (future) {
            futures.push_back(future);
        }
        return future;
    }

    // Helper: Create tracked future via map
    nimcp_future_t track_future(nimcp_future_t future) {
        if (future) {
            futures.push_back(future);
        }
        return future;
    }
};

//=============================================================================
// Basic Creation and Destruction Tests
//=============================================================================

/**
 * Test: Promise creation with valid size
 *
 * Verifies that promises can be created with various result sizes
 */
TEST_F(FutureTest, PromiseCreate_ValidSize_Success) {
    // Small result
    nimcp_promise_t p1 = create_promise(sizeof(int));
    ASSERT_NE(nullptr, p1);

    // Large result
    nimcp_promise_t p2 = create_promise(1024);
    ASSERT_NE(nullptr, p2);

    // Void result (0 size)
    nimcp_promise_t p3 = create_promise(0);
    ASSERT_NE(nullptr, p3);
}

/**
 * Test: Future retrieval from promise
 *
 * Verifies that futures can be obtained from promises
 */
TEST_F(FutureTest, FutureGet_FromPromise_Success) {
    nimcp_promise_t promise = create_promise(sizeof(int));
    ASSERT_NE(nullptr, promise);

    nimcp_future_t future = get_future(promise);
    ASSERT_NE(nullptr, future);
}

/**
 * Test: Multiple futures from same promise
 *
 * Verifies that multiple future handles can share same promise
 */
TEST_F(FutureTest, FutureGet_MultipleFutures_Success) {
    nimcp_promise_t promise = create_promise(sizeof(int));
    ASSERT_NE(nullptr, promise);

    // Get multiple future handles
    nimcp_future_t f1 = get_future(promise);
    nimcp_future_t f2 = get_future(promise);
    nimcp_future_t f3 = get_future(promise);

    ASSERT_NE(nullptr, f1);
    ASSERT_NE(nullptr, f2);
    ASSERT_NE(nullptr, f3);

    // All should reflect same state
    EXPECT_EQ(NIMCP_FUTURE_PENDING, nimcp_future_state(f1));
    EXPECT_EQ(NIMCP_FUTURE_PENDING, nimcp_future_state(f2));
    EXPECT_EQ(NIMCP_FUTURE_PENDING, nimcp_future_state(f3));
}

/**
 * Test: Memory management - no leaks
 *
 * Verifies that creation/destruction cycles don't leak memory
 */
TEST_F(FutureTest, MemoryManagement_CreateDestroyCycles_NoLeaks) {
    nimcp_future_stats_t stats_before, stats_after;
    nimcp_future_get_stats(&stats_before);

    // Create and destroy many promises/futures
    for (int i = 0; i < 100; ++i) {
        nimcp_promise_t promise = nimcp_promise_create(sizeof(int));
        nimcp_future_t future = nimcp_promise_get_future(promise);

        nimcp_future_destroy(future);
        nimcp_promise_destroy(promise);
    }

    nimcp_future_get_stats(&stats_after);

    // Active counts should be balanced
    EXPECT_EQ(stats_before.active_promises, stats_after.active_promises);
    EXPECT_EQ(stats_before.active_futures, stats_after.active_futures);
}

//=============================================================================
// State Transition Tests
//=============================================================================

/**
 * Test: Initial state is PENDING
 *
 * Verifies newly created futures start in PENDING state
 */
TEST_F(FutureTest, State_Initial_IsPending) {
    nimcp_promise_t promise = create_promise(sizeof(int));
    nimcp_future_t future = get_future(promise);

    EXPECT_EQ(NIMCP_FUTURE_PENDING, nimcp_future_state(future));
    EXPECT_FALSE(nimcp_future_is_ready(future));
}

/**
 * Test: Transition to COMPLETED state
 *
 * Verifies promise completion transitions to COMPLETED
 */
TEST_F(FutureTest, State_Complete_TransitionsToCompleted) {
    nimcp_promise_t promise = create_promise(sizeof(int));
    nimcp_future_t future = get_future(promise);

    int value = 42;
    ASSERT_EQ(NIMCP_SUCCESS, nimcp_promise_complete(promise, &value));

    EXPECT_EQ(NIMCP_FUTURE_COMPLETED, nimcp_future_state(future));
    EXPECT_TRUE(nimcp_future_is_ready(future));
}

/**
 * Test: Transition to FAILED state
 *
 * Verifies promise failure transitions to FAILED
 */
TEST_F(FutureTest, State_Fail_TransitionsToFailed) {
    nimcp_promise_t promise = create_promise(sizeof(int));
    nimcp_future_t future = get_future(promise);

    ASSERT_EQ(NIMCP_SUCCESS, nimcp_promise_fail(promise, NIMCP_ERROR_NO_MEMORY));

    EXPECT_EQ(NIMCP_FUTURE_FAILED, nimcp_future_state(future));
    EXPECT_TRUE(nimcp_future_is_ready(future));
    EXPECT_EQ(NIMCP_ERROR_NO_MEMORY, nimcp_future_get_error(future));
}

/**
 * Test: Transition to CANCELLED state
 *
 * Verifies cancellation transitions to CANCELLED
 */
TEST_F(FutureTest, State_Cancel_TransitionsToCancelled) {
    nimcp_promise_t promise = create_promise(sizeof(int));
    nimcp_future_t future = get_future(promise);

    ASSERT_TRUE(nimcp_future_cancel(future));

    EXPECT_EQ(NIMCP_FUTURE_CANCELLED, nimcp_future_state(future));
    EXPECT_TRUE(nimcp_future_is_ready(future));
}

/**
 * Test: Cannot complete after already completed
 *
 * Verifies state transitions are monotonic
 */
TEST_F(FutureTest, State_DoubleComplete_ReturnsError) {
    nimcp_promise_t promise = create_promise(sizeof(int));
    nimcp_future_t future = get_future(promise);

    int value = 42;
    ASSERT_EQ(NIMCP_SUCCESS, nimcp_promise_complete(promise, &value));

    // Second completion should fail
    int value2 = 99;
    EXPECT_NE(NIMCP_SUCCESS, nimcp_promise_complete(promise, &value2));

    // State should still be COMPLETED with original value
    EXPECT_EQ(NIMCP_FUTURE_COMPLETED, nimcp_future_state(future));
    int result;
    nimcp_future_get(future, &result);
    EXPECT_EQ(42, result);
}

/**
 * Test: Cannot cancel after completed
 *
 * Verifies completed futures cannot be cancelled
 */
TEST_F(FutureTest, State_CancelAfterComplete_Fails) {
    nimcp_promise_t promise = create_promise(sizeof(int));
    nimcp_future_t future = get_future(promise);

    int value = 42;
    ASSERT_EQ(NIMCP_SUCCESS, nimcp_promise_complete(promise, &value));

    // Cancellation should fail
    EXPECT_FALSE(nimcp_future_cancel(future));
    EXPECT_EQ(NIMCP_FUTURE_COMPLETED, nimcp_future_state(future));
}

//=============================================================================
// Value Setting and Retrieval Tests
//=============================================================================

/**
 * Test: Set and get integer value
 *
 * Verifies basic value passing through promise/future
 */
TEST_F(FutureTest, Value_SetGetInt_Success) {
    nimcp_promise_t promise = create_promise(sizeof(int));
    nimcp_future_t future = get_future(promise);

    int input = 42;
    ASSERT_EQ(NIMCP_SUCCESS, nimcp_promise_complete(promise, &input));

    int output = 0;
    ASSERT_EQ(NIMCP_SUCCESS, nimcp_future_get(future, &output));
    EXPECT_EQ(42, output);
}

/**
 * Test: Set and get struct value
 *
 * Verifies complex data types work correctly
 */
TEST_F(FutureTest, Value_SetGetStruct_Success) {
    struct TestData {
        int x;
        float y;
        char z;
    };

    nimcp_promise_t promise = create_promise(sizeof(TestData));
    nimcp_future_t future = get_future(promise);

    TestData input = {42, 3.14f, 'A'};
    ASSERT_EQ(NIMCP_SUCCESS, nimcp_promise_complete(promise, &input));

    TestData output = {};
    ASSERT_EQ(NIMCP_SUCCESS, nimcp_future_get(future, &output));
    EXPECT_EQ(42, output.x);
    EXPECT_FLOAT_EQ(3.14f, output.y);
    EXPECT_EQ('A', output.z);
}

/**
 * Test: Set and get array value
 *
 * Verifies arrays are copied correctly
 */
TEST_F(FutureTest, Value_SetGetArray_Success) {
    const size_t array_size = 10;
    nimcp_promise_t promise = create_promise(sizeof(int) * array_size);
    nimcp_future_t future = get_future(promise);

    int input[array_size];
    for (size_t i = 0; i < array_size; ++i) {
        input[i] = i * 2;
    }

    ASSERT_EQ(NIMCP_SUCCESS, nimcp_promise_complete(promise, input));

    int output[array_size] = {};
    ASSERT_EQ(NIMCP_SUCCESS, nimcp_future_get(future, output));

    for (size_t i = 0; i < array_size; ++i) {
        EXPECT_EQ(i * 2, output[i]);
    }
}

/**
 * Test: Get before complete returns error
 *
 * Verifies get() fails on pending futures
 */
TEST_F(FutureTest, Value_GetBeforeComplete_ReturnsError) {
    nimcp_promise_t promise = create_promise(sizeof(int));
    nimcp_future_t future = get_future(promise);

    int output;
    EXPECT_NE(NIMCP_SUCCESS, nimcp_future_get(future, &output));
}

//=============================================================================
// Error Handling Tests
//=============================================================================

/**
 * Test: Set error and retrieve it
 *
 * Verifies error codes propagate correctly
 */
TEST_F(FutureTest, Error_SetAndRetrieve_Success) {
    nimcp_promise_t promise = create_promise(sizeof(int));
    nimcp_future_t future = get_future(promise);

    ASSERT_EQ(NIMCP_SUCCESS, nimcp_promise_fail(promise, NIMCP_ERROR_NO_MEMORY));

    EXPECT_EQ(NIMCP_FUTURE_FAILED, nimcp_future_state(future));
    EXPECT_EQ(NIMCP_ERROR_NO_MEMORY, nimcp_future_get_error(future));
}

/**
 * Test: Multiple error codes
 *
 * Verifies various error codes work correctly
 */
TEST_F(FutureTest, Error_DifferentErrorCodes_Success) {
    nimcp_error_t errors[] = {
        NIMCP_ERROR_NO_MEMORY,
        NIMCP_ERROR_INVALID_PARAMETER,
        NIMCP_ERROR_TIMEOUT,
        NIMCP_ERROR_INVALID_STATE
    };

    for (auto err : errors) {
        nimcp_promise_t promise = create_promise(sizeof(int));
        nimcp_future_t future = get_future(promise);

        ASSERT_EQ(NIMCP_SUCCESS, nimcp_promise_fail(promise, err));
        EXPECT_EQ(err, nimcp_future_get_error(future));
    }
}

/**
 * Test: Get error on successful future returns SUCCESS
 *
 * Verifies error code is SUCCESS for completed futures
 */
TEST_F(FutureTest, Error_GetOnSuccess_ReturnsSuccess) {
    nimcp_promise_t promise = create_promise(sizeof(int));
    nimcp_future_t future = get_future(promise);

    int value = 42;
    ASSERT_EQ(NIMCP_SUCCESS, nimcp_promise_complete(promise, &value));

    EXPECT_EQ(NIMCP_SUCCESS, nimcp_future_get_error(future));
}

//=============================================================================
// Blocking Wait Tests
//=============================================================================

/**
 * Test: Wait on already completed future returns immediately
 *
 * Verifies fast-path for already-ready futures
 */
TEST_F(FutureTest, Wait_AlreadyCompleted_Immediate) {
    nimcp_promise_t promise = create_promise(sizeof(int));
    nimcp_future_t future = get_future(promise);

    int value = 42;
    ASSERT_EQ(NIMCP_SUCCESS, nimcp_promise_complete(promise, &value));

    auto start = std::chrono::steady_clock::now();
    bool result = nimcp_future_wait(future);
    auto duration = std::chrono::steady_clock::now() - start;

    EXPECT_TRUE(result);
    // Should be very fast (< 1ms)
    EXPECT_LT(duration.count(), 1000000);  // 1ms in nanoseconds
}

/**
 * Test: Wait blocks until completion
 *
 * Verifies wait() blocks and wakes on completion
 */
TEST_F(FutureTest, Wait_BlocksUntilComplete_Success) {
    nimcp_promise_t promise = create_promise(sizeof(int));
    nimcp_future_t future = get_future(promise);

    std::atomic<bool> completed{false};

    // Waiter thread
    std::thread waiter([&]() {
        bool result = nimcp_future_wait(future);
        EXPECT_TRUE(result);
        completed.store(true);
    });

    // Give waiter time to block
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    EXPECT_FALSE(completed.load());

    // Complete the promise
    int value = 42;
    nimcp_promise_complete(promise, &value);

    waiter.join();
    EXPECT_TRUE(completed.load());
}

/**
 * Test: Wait on failed future returns false
 *
 * Verifies wait distinguishes success/failure
 */
TEST_F(FutureTest, Wait_OnFailed_ReturnsFalse) {
    nimcp_promise_t promise = create_promise(sizeof(int));
    nimcp_future_t future = get_future(promise);

    ASSERT_EQ(NIMCP_SUCCESS, nimcp_promise_fail(promise, NIMCP_ERROR_NO_MEMORY));

    EXPECT_FALSE(nimcp_future_wait(future));
}

/**
 * Test: Wait on cancelled future returns false
 *
 * Verifies wait handles cancellation
 */
TEST_F(FutureTest, Wait_OnCancelled_ReturnsFalse) {
    nimcp_promise_t promise = create_promise(sizeof(int));
    nimcp_future_t future = get_future(promise);

    ASSERT_TRUE(nimcp_future_cancel(future));

    EXPECT_FALSE(nimcp_future_wait(future));
}

//=============================================================================
// Timed Wait Tests
//=============================================================================

/**
 * Test: Wait with timeout on ready future succeeds immediately
 *
 * Verifies timeout wait fast-path
 */
TEST_F(FutureTest, WaitTimeout_AlreadyReady_Immediate) {
    nimcp_promise_t promise = create_promise(sizeof(int));
    nimcp_future_t future = get_future(promise);

    int value = 42;
    ASSERT_EQ(NIMCP_SUCCESS, nimcp_promise_complete(promise, &value));

    EXPECT_TRUE(nimcp_future_wait_timeout(future, 1000));
}

/**
 * Test: Wait timeout expires correctly
 *
 * Verifies timeout mechanism works
 */
TEST_F(FutureTest, WaitTimeout_Expires_ReturnsFalse) {
    nimcp_promise_t promise = create_promise(sizeof(int));
    nimcp_future_t future = get_future(promise);

    auto start = std::chrono::steady_clock::now();
    bool result = nimcp_future_wait_timeout(future, 100);  // 100ms timeout
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start
    );

    EXPECT_FALSE(result);
    // Should take approximately 100ms (allow some margin)
    EXPECT_GE(duration.count(), 90);
    EXPECT_LE(duration.count(), 150);
}

/**
 * Test: Wait timeout succeeds when completed in time
 *
 * Verifies completion before timeout succeeds
 */
TEST_F(FutureTest, WaitTimeout_CompletesInTime_Success) {
    nimcp_promise_t promise = create_promise(sizeof(int));
    nimcp_future_t future = get_future(promise);

    std::atomic<bool> wait_result{false};

    // Waiter thread with 500ms timeout
    std::thread waiter([&]() {
        wait_result.store(nimcp_future_wait_timeout(future, 500));
    });

    // Complete after 50ms
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    int value = 42;
    nimcp_promise_complete(promise, &value);

    waiter.join();
    EXPECT_TRUE(wait_result.load());
}

/**
 * Test: Zero timeout is immediate check
 *
 * Verifies 0 timeout doesn't block
 */
TEST_F(FutureTest, WaitTimeout_ZeroTimeout_ImmediateCheck) {
    nimcp_promise_t promise = create_promise(sizeof(int));
    nimcp_future_t future = get_future(promise);

    auto start = std::chrono::steady_clock::now();
    bool result = nimcp_future_wait_timeout(future, 0);
    auto duration = std::chrono::steady_clock::now() - start;

    EXPECT_FALSE(result);
    // Should be very fast (< 1ms)
    EXPECT_LT(duration.count(), 1000000);
}

//=============================================================================
// Non-blocking Check Tests
//=============================================================================

/**
 * Test: is_ready on pending future returns false
 *
 * Verifies non-blocking check for pending state
 */
TEST_F(FutureTest, IsReady_Pending_ReturnsFalse) {
    nimcp_promise_t promise = create_promise(sizeof(int));
    nimcp_future_t future = get_future(promise);

    EXPECT_FALSE(nimcp_future_is_ready(future));
}

/**
 * Test: is_ready on completed future returns true
 *
 * Verifies non-blocking check for completion
 */
TEST_F(FutureTest, IsReady_Completed_ReturnsTrue) {
    nimcp_promise_t promise = create_promise(sizeof(int));
    nimcp_future_t future = get_future(promise);

    int value = 42;
    nimcp_promise_complete(promise, &value);

    EXPECT_TRUE(nimcp_future_is_ready(future));
}

/**
 * Test: is_ready on failed future returns true
 *
 * Verifies ready check for all terminal states
 */
TEST_F(FutureTest, IsReady_Failed_ReturnsTrue) {
    nimcp_promise_t promise = create_promise(sizeof(int));
    nimcp_future_t future = get_future(promise);

    nimcp_promise_fail(promise, NIMCP_ERROR_NO_MEMORY);

    EXPECT_TRUE(nimcp_future_is_ready(future));
}

/**
 * Test: is_ready is non-blocking
 *
 * Verifies is_ready doesn't block
 */
TEST_F(FutureTest, IsReady_NonBlocking_Fast) {
    nimcp_promise_t promise = create_promise(sizeof(int));
    nimcp_future_t future = get_future(promise);

    auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < 1000; ++i) {
        nimcp_future_is_ready(future);
    }
    auto duration = std::chrono::steady_clock::now() - start;

    // 1000 checks should be very fast (< 1ms)
    EXPECT_LT(duration.count(), 1000000);
}

//=============================================================================
// Cancellation Tests
//=============================================================================

/**
 * Test: Cancel pending future succeeds
 *
 * Verifies cancellation works on pending futures
 */
TEST_F(FutureTest, Cancel_Pending_Success) {
    nimcp_promise_t promise = create_promise(sizeof(int));
    nimcp_future_t future = get_future(promise);

    EXPECT_TRUE(nimcp_future_cancel(future));
    EXPECT_EQ(NIMCP_FUTURE_CANCELLED, nimcp_future_state(future));
}

/**
 * Test: Cancel wakes waiting threads
 *
 * Verifies cancellation unblocks waiters
 */
TEST_F(FutureTest, Cancel_WakesWaiters_Success) {
    nimcp_promise_t promise = create_promise(sizeof(int));
    nimcp_future_t future = get_future(promise);

    std::atomic<bool> wait_finished{false};

    // Waiter thread
    std::thread waiter([&]() {
        nimcp_future_wait(future);
        wait_finished.store(true);
    });

    // Give waiter time to block
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    // Cancel should wake waiter
    nimcp_future_cancel(future);

    waiter.join();
    EXPECT_TRUE(wait_finished.load());
}

/**
 * Test: Double cancel is idempotent
 *
 * Verifies multiple cancels are safe
 */
TEST_F(FutureTest, Cancel_DoubleCancle_Idempotent) {
    nimcp_promise_t promise = create_promise(sizeof(int));
    nimcp_future_t future = get_future(promise);

    EXPECT_TRUE(nimcp_future_cancel(future));
    EXPECT_FALSE(nimcp_future_cancel(future));  // Second cancel fails
    EXPECT_EQ(NIMCP_FUTURE_CANCELLED, nimcp_future_state(future));
}

//=============================================================================
// Callback Tests
//=============================================================================

/**
 * Test: Callback invoked on completion
 *
 * Verifies then() callbacks are executed
 */
TEST_F(FutureTest, Callback_OnComplete_Invoked) {
    nimcp_promise_t promise = create_promise(sizeof(int));
    nimcp_future_t future = get_future(promise);

    std::atomic<bool> callback_called{false};
    std::atomic<int> callback_result{0};

    auto callback = [](const void* result, nimcp_error_t error, void* user_data) {
        auto called = static_cast<std::atomic<bool>*>(user_data);
        called->store(true);
    };

    ASSERT_EQ(NIMCP_SUCCESS, nimcp_future_then(future, callback, &callback_called));

    int value = 42;
    nimcp_promise_complete(promise, &value);

    // Give callback time to execute
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    EXPECT_TRUE(callback_called.load());
}

/**
 * Test: Callback receives correct result
 *
 * Verifies callback gets the actual result value
 */
TEST_F(FutureTest, Callback_ReceivesResult_Correct) {
    nimcp_promise_t promise = create_promise(sizeof(int));
    nimcp_future_t future = get_future(promise);

    std::atomic<int> callback_result{0};

    auto callback = [](const void* result, nimcp_error_t error, void* user_data) {
        if (result && error == NIMCP_SUCCESS) {
            auto result_ptr = static_cast<std::atomic<int>*>(user_data);
            result_ptr->store(*static_cast<const int*>(result));
        }
    };

    ASSERT_EQ(NIMCP_SUCCESS, nimcp_future_then(future, callback, &callback_result));

    int value = 42;
    nimcp_promise_complete(promise, &value);

    // Give callback time to execute
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    EXPECT_EQ(42, callback_result.load());
}

/**
 * Test: Callback invoked immediately if already complete
 *
 * Verifies callbacks execute immediately for ready futures
 */
TEST_F(FutureTest, Callback_AlreadyComplete_InvokedImmediately) {
    nimcp_promise_t promise = create_promise(sizeof(int));
    nimcp_future_t future = get_future(promise);

    int value = 42;
    nimcp_promise_complete(promise, &value);

    std::atomic<bool> callback_called{false};

    auto callback = [](const void* result, nimcp_error_t error, void* user_data) {
        auto called = static_cast<std::atomic<bool>*>(user_data);
        called->store(true);
    };

    ASSERT_EQ(NIMCP_SUCCESS, nimcp_future_then(future, callback, &callback_called));

    // Should be called immediately (or very quickly)
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    EXPECT_TRUE(callback_called.load());
}

/**
 * Test: Multiple callbacks on same future
 *
 * Verifies multiple then() calls all execute
 */
TEST_F(FutureTest, Callback_Multiple_AllInvoked) {
    nimcp_promise_t promise = create_promise(sizeof(int));
    nimcp_future_t future = get_future(promise);

    std::atomic<int> counter{0};

    auto callback = [](const void* result, nimcp_error_t error, void* user_data) {
        auto count = static_cast<std::atomic<int>*>(user_data);
        count->fetch_add(1);
    };

    // Register 3 callbacks
    ASSERT_EQ(NIMCP_SUCCESS, nimcp_future_then(future, callback, &counter));
    ASSERT_EQ(NIMCP_SUCCESS, nimcp_future_then(future, callback, &counter));
    ASSERT_EQ(NIMCP_SUCCESS, nimcp_future_then(future, callback, &counter));

    int value = 42;
    nimcp_promise_complete(promise, &value);

    // Give callbacks time to execute
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    EXPECT_EQ(3, counter.load());
}

//=============================================================================
// Combinator: ALL Tests
//=============================================================================

/**
 * Test: future_all waits for all to complete
 *
 * Verifies all combinator completes when all inputs complete
 */
TEST_F(FutureTest, CombinatorAll_AllComplete_Success) {
    // Create 3 promises
    nimcp_promise_t p1 = create_promise(sizeof(int));
    nimcp_promise_t p2 = create_promise(sizeof(int));
    nimcp_promise_t p3 = create_promise(sizeof(int));

    nimcp_future_t f1 = get_future(p1);
    nimcp_future_t f2 = get_future(p2);
    nimcp_future_t f3 = get_future(p3);

    nimcp_future_t futures[] = {f1, f2, f3};
    nimcp_future_t all = track_future(nimcp_future_all(futures, 3));
    ASSERT_NE(nullptr, all);

    // Should not be ready yet
    EXPECT_FALSE(nimcp_future_is_ready(all));

    // Complete them one by one
    int v1 = 1;
    nimcp_promise_complete(p1, &v1);
    EXPECT_FALSE(nimcp_future_is_ready(all));

    int v2 = 2;
    nimcp_promise_complete(p2, &v2);
    EXPECT_FALSE(nimcp_future_is_ready(all));

    int v3 = 3;
    nimcp_promise_complete(p3, &v3);

    // Now all should be ready
    EXPECT_TRUE(nimcp_future_wait(all));
}

/**
 * Test: future_all handles failures
 *
 * Verifies all combinator fails if any input fails (standard all semantics)
 */
TEST_F(FutureTest, CombinatorAll_OneFails_StillCompletes) {
    nimcp_promise_t p1 = create_promise(sizeof(int));
    nimcp_promise_t p2 = create_promise(sizeof(int));

    nimcp_future_t f1 = get_future(p1);
    nimcp_future_t f2 = get_future(p2);

    nimcp_future_t futures[] = {f1, f2};
    nimcp_future_t all = track_future(nimcp_future_all(futures, 2));
    ASSERT_NE(nullptr, all);

    int v1 = 1;
    nimcp_promise_complete(p1, &v1);
    nimcp_promise_fail(p2, NIMCP_ERROR_NO_MEMORY);

    // wait() returns false because one failed - but it should reach terminal state
    nimcp_future_wait_timeout(all, 100);  // Wait for terminal state
    EXPECT_TRUE(nimcp_future_is_ready(all));  // Verify terminal state reached
    EXPECT_EQ(NIMCP_FUTURE_FAILED, nimcp_future_state(all));  // Should be FAILED
    EXPECT_EQ(NIMCP_ERROR_NO_MEMORY, nimcp_future_get_error(all));  // First error propagated
}

//=============================================================================
// Combinator: ANY Tests
//=============================================================================

/**
 * Test: future_any completes on first completion
 *
 * Verifies any combinator returns when first future completes
 */
TEST_F(FutureTest, CombinatorAny_FirstComplete_Success) {
    nimcp_promise_t p1 = create_promise(sizeof(int));
    nimcp_promise_t p2 = create_promise(sizeof(int));
    nimcp_promise_t p3 = create_promise(sizeof(int));

    nimcp_future_t f1 = get_future(p1);
    nimcp_future_t f2 = get_future(p2);
    nimcp_future_t f3 = get_future(p3);

    nimcp_future_t futures[] = {f1, f2, f3};
    nimcp_future_t any = track_future(nimcp_future_any(futures, 3));
    ASSERT_NE(nullptr, any);

    // Complete second one first
    int v2 = 2;
    nimcp_promise_complete(p2, &v2);

    // Should be ready now
    EXPECT_TRUE(nimcp_future_wait(any));

    // Result should contain index 1 (second future)
    size_t winner_index;
    ASSERT_EQ(NIMCP_SUCCESS, nimcp_future_get(any, &winner_index));
    EXPECT_EQ(1u, winner_index);
}

/**
 * Test: future_any with all pending times out
 *
 * Verifies any combinator doesn't complete if all pending
 */
TEST_F(FutureTest, CombinatorAny_AllPending_Timeout) {
    nimcp_promise_t p1 = create_promise(sizeof(int));
    nimcp_promise_t p2 = create_promise(sizeof(int));

    nimcp_future_t f1 = get_future(p1);
    nimcp_future_t f2 = get_future(p2);

    nimcp_future_t futures[] = {f1, f2};
    nimcp_future_t any = track_future(nimcp_future_any(futures, 2));
    ASSERT_NE(nullptr, any);

    // Should timeout since none complete
    EXPECT_FALSE(nimcp_future_wait_timeout(any, 50));
}

//=============================================================================
// Combinator: MAP Tests
//=============================================================================

/**
 * Test: future_map transforms value
 *
 * Verifies map combinator applies transformation
 */
TEST_F(FutureTest, CombinatorMap_TransformValue_Success) {
    nimcp_promise_t promise = create_promise(sizeof(int));
    nimcp_future_t source = get_future(promise);

    // Transform: multiply by 2
    auto double_it = [](const void* input, void* output, void* user_data) -> nimcp_error_t {
        *(int*)output = *(const int*)input * 2;
        return NIMCP_SUCCESS;
    };

    nimcp_future_t mapped = track_future(
        nimcp_future_map(source, double_it, sizeof(int), nullptr)
    );
    ASSERT_NE(nullptr, mapped);

    // Complete source
    int value = 21;
    nimcp_promise_complete(promise, &value);

    // Wait and get transformed result
    EXPECT_TRUE(nimcp_future_wait(mapped));
    int result;
    ASSERT_EQ(NIMCP_SUCCESS, nimcp_future_get(mapped, &result));
    EXPECT_EQ(42, result);
}

/**
 * Test: future_map type conversion
 *
 * Verifies map can change result type
 */
TEST_F(FutureTest, CombinatorMap_TypeConversion_Success) {
    nimcp_promise_t promise = create_promise(sizeof(int));
    nimcp_future_t source = get_future(promise);

    // Transform: int to float
    auto int_to_float = [](const void* input, void* output, void* user_data) -> nimcp_error_t {
        *(float*)output = (float)(*(const int*)input);
        return NIMCP_SUCCESS;
    };

    nimcp_future_t mapped = track_future(
        nimcp_future_map(source, int_to_float, sizeof(float), nullptr)
    );
    ASSERT_NE(nullptr, mapped);

    int value = 42;
    nimcp_promise_complete(promise, &value);

    EXPECT_TRUE(nimcp_future_wait(mapped));
    float result;
    ASSERT_EQ(NIMCP_SUCCESS, nimcp_future_get(mapped, &result));
    EXPECT_FLOAT_EQ(42.0f, result);
}

/**
 * Test: future_map error propagation
 *
 * Verifies map propagates source errors
 */
TEST_F(FutureTest, CombinatorMap_ErrorPropagation_Success) {
    nimcp_promise_t promise = create_promise(sizeof(int));
    nimcp_future_t source = get_future(promise);

    auto identity = [](const void* input, void* output, void* user_data) -> nimcp_error_t {
        *(int*)output = *(const int*)input;
        return NIMCP_SUCCESS;
    };

    nimcp_future_t mapped = track_future(
        nimcp_future_map(source, identity, sizeof(int), nullptr)
    );
    ASSERT_NE(nullptr, mapped);

    // Fail source
    nimcp_promise_fail(promise, NIMCP_ERROR_NO_MEMORY);

    // Mapped should also fail
    EXPECT_FALSE(nimcp_future_wait(mapped));
    EXPECT_EQ(NIMCP_ERROR_NO_MEMORY, nimcp_future_get_error(mapped));
}

//=============================================================================
// Thread Safety Tests
//=============================================================================

/**
 * Test: Set from one thread, wait from another
 *
 * Verifies basic cross-thread communication
 */
TEST_F(FutureTest, ThreadSafety_CrossThreadCompletion_Success) {
    nimcp_promise_t promise = create_promise(sizeof(int));
    nimcp_future_t future = get_future(promise);

    std::atomic<int> result{0};

    // Consumer thread
    std::thread consumer([&]() {
        nimcp_future_wait(future);
        int value;
        nimcp_future_get(future, &value);
        result.store(value);
    });

    // Producer thread
    std::thread producer([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        int value = 42;
        nimcp_promise_complete(promise, &value);
    });

    producer.join();
    consumer.join();

    EXPECT_EQ(42, result.load());
}

/**
 * Test: Multiple threads waiting on same future
 *
 * Verifies multiple waiters are all woken
 */
TEST_F(FutureTest, ThreadSafety_MultipleWaiters_AllWoken) {
    nimcp_promise_t promise = create_promise(sizeof(int));
    nimcp_future_t future = get_future(promise);

    const int num_waiters = 5;
    std::atomic<int> wake_count{0};
    std::vector<std::thread> waiters;

    for (int i = 0; i < num_waiters; ++i) {
        waiters.emplace_back([&]() {
            if (nimcp_future_wait(future)) {
                wake_count.fetch_add(1);
            }
        });
    }

    // Give waiters time to block
    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    // Complete promise
    int value = 42;
    nimcp_promise_complete(promise, &value);

    for (auto& t : waiters) {
        t.join();
    }

    EXPECT_EQ(num_waiters, wake_count.load());
}

/**
 * Test: Concurrent state checks
 *
 * Verifies concurrent is_ready calls are safe
 */
TEST_F(FutureTest, ThreadSafety_ConcurrentStateChecks_Safe) {
    nimcp_promise_t promise = create_promise(sizeof(int));
    nimcp_future_t future = get_future(promise);

    std::atomic<bool> stop{false};
    std::atomic<int> check_count{0};

    // Multiple checker threads
    std::vector<std::thread> checkers;
    for (int i = 0; i < 3; ++i) {
        checkers.emplace_back([&]() {
            while (!stop.load()) {
                nimcp_future_is_ready(future);
                check_count.fetch_add(1);
            }
        });
    }

    // Let them run for a bit
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    // Complete and stop
    int value = 42;
    nimcp_promise_complete(promise, &value);
    stop.store(true);

    for (auto& t : checkers) {
        t.join();
    }

    // Should have performed many checks without crashes
    EXPECT_GT(check_count.load(), 100);
}

//=============================================================================
// Statistics Tests
//=============================================================================

/**
 * Test: Statistics tracking creation/destruction
 *
 * Verifies statistics are updated correctly
 */
TEST_F(FutureTest, Statistics_CreationDestruction_Tracked) {
    nimcp_future_stats_t stats;

    // Create some promises and futures
    for (int i = 0; i < 10; ++i) {
        nimcp_promise_t p = create_promise(sizeof(int));
        nimcp_future_t f = get_future(p);
    }

    nimcp_future_get_stats(&stats);
    EXPECT_GE(stats.promises_created, 10u);
    EXPECT_GE(stats.futures_created, 10u);
}

/**
 * Test: Statistics tracking completions
 *
 * Verifies completion stats are updated
 */
TEST_F(FutureTest, Statistics_Completions_Tracked) {
    nimcp_future_reset_stats();

    nimcp_promise_t p1 = create_promise(sizeof(int));
    nimcp_promise_t p2 = create_promise(sizeof(int));
    nimcp_promise_t p3 = create_promise(sizeof(int));

    int value = 42;
    nimcp_promise_complete(p1, &value);
    nimcp_promise_fail(p2, NIMCP_ERROR_NO_MEMORY);
    nimcp_future_t f3 = get_future(p3);
    nimcp_future_cancel(f3);

    nimcp_future_stats_t stats;
    nimcp_future_get_stats(&stats);

    EXPECT_EQ(1u, stats.completions);
    EXPECT_EQ(1u, stats.failures);
    EXPECT_EQ(1u, stats.cancellations);
}

/**
 * Test: Statistics reset
 *
 * Verifies stats can be reset
 */
TEST_F(FutureTest, Statistics_Reset_ClearsCounters) {
    // Create some activity
    nimcp_promise_t p = create_promise(sizeof(int));
    int value = 42;
    nimcp_promise_complete(p, &value);

    nimcp_future_reset_stats();

    nimcp_future_stats_t stats;
    nimcp_future_get_stats(&stats);

    EXPECT_EQ(0u, stats.completions);
    EXPECT_EQ(0u, stats.failures);
    EXPECT_EQ(0u, stats.cancellations);
}

//=============================================================================
// Stress Tests
//=============================================================================

/**
 * Test: Many concurrent operations
 *
 * Verifies system handles high concurrency
 */
TEST_F(FutureTest, Stress_ManyConcurrentOperations_NoErrors) {
    const int num_operations = 100;
    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};

    for (int i = 0; i < num_operations; ++i) {
        threads.emplace_back([&, i]() {
            nimcp_promise_t p = nimcp_promise_create(sizeof(int));
            nimcp_future_t f = nimcp_promise_get_future(p);

            int value = i;
            nimcp_promise_complete(p, &value);

            if (nimcp_future_wait(f)) {
                int result;
                if (nimcp_future_get(f, &result) == NIMCP_SUCCESS && result == i) {
                    success_count.fetch_add(1);
                }
            }

            nimcp_future_destroy(f);
            nimcp_promise_destroy(p);
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(num_operations, success_count.load());
}

/**
 * Test: Rapid create/destroy cycles
 *
 * Verifies no resource leaks under pressure
 */
TEST_F(FutureTest, Stress_RapidCreateDestroy_NoLeaks) {
    nimcp_future_stats_t before, after;
    nimcp_future_get_stats(&before);

    for (int i = 0; i < 1000; ++i) {
        nimcp_promise_t p = nimcp_promise_create(sizeof(int));
        nimcp_future_t f = nimcp_promise_get_future(p);
        nimcp_future_destroy(f);
        nimcp_promise_destroy(p);
    }

    nimcp_future_get_stats(&after);

    // Active counts should be balanced
    EXPECT_EQ(before.active_promises, after.active_promises);
    EXPECT_EQ(before.active_futures, after.active_futures);
}

//=============================================================================
// Edge Cases
//=============================================================================

/**
 * Test: Destroy promise before future
 *
 * Verifies destruction order doesn't matter
 */
TEST_F(FutureTest, EdgeCase_DestroyPromiseFirst_Safe) {
    nimcp_promise_t promise = create_promise(sizeof(int));
    nimcp_future_t future = get_future(promise);

    int value = 42;
    nimcp_promise_complete(promise, &value);

    // Destroy promise first
    auto it = std::find(promises.begin(), promises.end(), promise);
    if (it != promises.end()) {
        promises.erase(it);
    }
    nimcp_promise_destroy(promise);

    // Future should still work
    EXPECT_TRUE(nimcp_future_is_ready(future));
    int result;
    EXPECT_EQ(NIMCP_SUCCESS, nimcp_future_get(future, &result));
    EXPECT_EQ(42, result);
}

/**
 * Test: Large result size
 *
 * Verifies large data transfers work
 */
TEST_F(FutureTest, EdgeCase_LargeResult_Success) {
    const size_t large_size = 1024 * 1024;  // 1MB
    nimcp_promise_t promise = create_promise(large_size);
    nimcp_future_t future = get_future(promise);

    std::vector<uint8_t> input(large_size);
    for (size_t i = 0; i < large_size; ++i) {
        input[i] = i % 256;
    }

    ASSERT_EQ(NIMCP_SUCCESS, nimcp_promise_complete(promise, input.data()));

    std::vector<uint8_t> output(large_size);
    ASSERT_EQ(NIMCP_SUCCESS, nimcp_future_get(future, output.data()));

    EXPECT_EQ(input, output);
}

/**
 * Test: NULL pointer handling
 *
 * Verifies NULL checks work correctly
 */
TEST_F(FutureTest, EdgeCase_NullPointers_Handled) {
    // NULL future operations should not crash
    nimcp_future_destroy(nullptr);
    nimcp_promise_destroy(nullptr);

    EXPECT_EQ(NIMCP_FUTURE_PENDING, nimcp_future_state(nullptr));
    EXPECT_FALSE(nimcp_future_is_ready(nullptr));
}

