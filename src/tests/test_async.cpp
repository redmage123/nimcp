/**
 * @file test_async.cpp
 * @brief Unit tests for async futures (v2.7.0 Enhancement 4)
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>

extern "C" {
#include "utils/async/nimcp_async.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/time/nimcp_time.h"
}

class AsyncTest : public ::testing::Test {
protected:
    void SetUp() override {
        nimcp_memory_init();
    }

    void TearDown() override {
        // Cleanup
    }

    // Helper to simulate async work
    static void simulate_work_ms(uint32_t duration_ms) {
        std::this_thread::sleep_for(std::chrono::milliseconds(duration_ms));
    }
};

//=============================================================================
// Future Creation and Lifecycle Tests
//=============================================================================

TEST_F(AsyncTest, FutureCreation) {
    nimcp_future_t future = nimcp_future_create();

    ASSERT_NE(future, nullptr);
    EXPECT_EQ(nimcp_future_status(future), NIMCP_FUTURE_PENDING);

    nimcp_future_destroy(future, true);
}

TEST_F(AsyncTest, FutureStatusTransitions) {
    nimcp_future_t future = nimcp_future_create();
    ASSERT_NE(future, nullptr);

    // Initially pending
    EXPECT_EQ(nimcp_future_status(future), NIMCP_FUTURE_PENDING);

    // Mark as running
    nimcp_future_set_running(future);
    EXPECT_EQ(nimcp_future_status(future), NIMCP_FUTURE_RUNNING);

    // Complete successfully
    float result = 42.0f;
    nimcp_future_complete(future, &result, sizeof(float));
    EXPECT_EQ(nimcp_future_status(future), NIMCP_FUTURE_COMPLETED);

    nimcp_future_destroy(future, true);
}

TEST_F(AsyncTest, FutureError) {
    nimcp_future_t future = nimcp_future_create();
    ASSERT_NE(future, nullptr);

    const char* error_msg = "Test error";
    nimcp_future_set_error(future, error_msg);

    EXPECT_EQ(nimcp_future_status(future), NIMCP_FUTURE_ERROR);

    nimcp_future_destroy(future, true);
}

TEST_F(AsyncTest, FutureCancellation) {
    nimcp_future_t future = nimcp_future_create();
    ASSERT_NE(future, nullptr);

    bool success = nimcp_future_cancel(future);
    EXPECT_TRUE(success);
    EXPECT_EQ(nimcp_future_status(future), NIMCP_FUTURE_CANCELLED);

    nimcp_future_destroy(future, true);
}

TEST_F(AsyncTest, FutureCancellationWhileRunning) {
    nimcp_future_t future = nimcp_future_create();
    ASSERT_NE(future, nullptr);

    nimcp_future_set_running(future);

    // Should still be able to cancel
    bool success = nimcp_future_cancel(future);
    EXPECT_TRUE(success);
    EXPECT_EQ(nimcp_future_status(future), NIMCP_FUTURE_CANCELLED);

    nimcp_future_destroy(future, true);
}

TEST_F(AsyncTest, CannotCancelCompletedFuture) {
    nimcp_future_t future = nimcp_future_create();
    ASSERT_NE(future, nullptr);

    float result = 42.0f;
    nimcp_future_complete(future, &result, sizeof(float));

    // Cannot cancel completed future
    bool success = nimcp_future_cancel(future);
    EXPECT_FALSE(success);
    EXPECT_EQ(nimcp_future_status(future), NIMCP_FUTURE_COMPLETED);

    nimcp_future_destroy(future, true);
}

//=============================================================================
// Future Wait Tests
//=============================================================================

TEST_F(AsyncTest, WaitForCompletedFuture) {
    nimcp_future_t future = nimcp_future_create();
    ASSERT_NE(future, nullptr);

    float result = 123.0f;
    nimcp_future_complete(future, &result, sizeof(float));

    // Wait should return immediately
    uint64_t start = nimcp_time_get_ms();
    bool success = nimcp_future_wait(future, 1000);  // 1 second timeout
    uint64_t elapsed = nimcp_time_get_ms() - start;

    EXPECT_TRUE(success);
    EXPECT_LT(elapsed, 100);  // Should be much less than timeout

    nimcp_future_destroy(future, true);
}

TEST_F(AsyncTest, WaitTimeout) {
    nimcp_future_t future = nimcp_future_create();
    ASSERT_NE(future, nullptr);

    // Don't complete the future
    bool success = nimcp_future_wait(future, 50);  // 50ms timeout

    EXPECT_FALSE(success);  // Should timeout
    EXPECT_EQ(nimcp_future_status(future), NIMCP_FUTURE_PENDING);

    nimcp_future_destroy(future, true);
}

TEST_F(AsyncTest, WaitInfinite) {
    nimcp_future_t future = nimcp_future_create();
    ASSERT_NE(future, nullptr);

    // Complete in background thread
    std::thread worker([future]() {
        simulate_work_ms(100);
        float result = 42.0f;
        nimcp_future_complete(future, &result, sizeof(float));
    });

    // Wait with 0 timeout = infinite
    bool success = nimcp_future_wait(future, 0);

    EXPECT_TRUE(success);
    EXPECT_EQ(nimcp_future_status(future), NIMCP_FUTURE_COMPLETED);

    worker.join();
    nimcp_future_destroy(future, true);
}

TEST_F(AsyncTest, IsDone) {
    nimcp_future_t future = nimcp_future_create();
    ASSERT_NE(future, nullptr);

    EXPECT_FALSE(nimcp_future_is_done(future));

    float result = 42.0f;
    nimcp_future_complete(future, &result, sizeof(float));

    EXPECT_TRUE(nimcp_future_is_done(future));

    nimcp_future_destroy(future, true);
}

TEST_F(AsyncTest, IsDoneOnError) {
    nimcp_future_t future = nimcp_future_create();
    ASSERT_NE(future, nullptr);

    nimcp_future_set_error(future, "Error");

    EXPECT_TRUE(nimcp_future_is_done(future));

    nimcp_future_destroy(future, true);
}

TEST_F(AsyncTest, IsDoneOnCancellation) {
    nimcp_future_t future = nimcp_future_create();
    ASSERT_NE(future, nullptr);

    nimcp_future_cancel(future);

    EXPECT_TRUE(nimcp_future_is_done(future));

    nimcp_future_destroy(future, true);
}

//=============================================================================
// Future Result Tests
//=============================================================================

TEST_F(AsyncTest, GetResultSuccess) {
    nimcp_future_t future = nimcp_future_create();
    ASSERT_NE(future, nullptr);

    float expected = 3.14159f;
    nimcp_future_complete(future, &expected, sizeof(float));

    float actual;
    bool success = nimcp_future_get_result(future, &actual, sizeof(float));

    EXPECT_TRUE(success);
    EXPECT_FLOAT_EQ(actual, expected);

    nimcp_future_destroy(future, true);
}

TEST_F(AsyncTest, GetResultBeforeCompletion) {
    nimcp_future_t future = nimcp_future_create();
    ASSERT_NE(future, nullptr);

    float result;
    bool success = nimcp_future_get_result(future, &result, sizeof(float));

    EXPECT_FALSE(success);  // Not yet completed

    nimcp_future_destroy(future, true);
}

TEST_F(AsyncTest, GetResultAfterError) {
    nimcp_future_t future = nimcp_future_create();
    ASSERT_NE(future, nullptr);

    nimcp_future_set_error(future, "Error");

    float result;
    bool success = nimcp_future_get_result(future, &result, sizeof(float));

    EXPECT_FALSE(success);

    nimcp_future_destroy(future, true);
}

TEST_F(AsyncTest, GetResultBufferTooSmall) {
    nimcp_future_t future = nimcp_future_create();
    ASSERT_NE(future, nullptr);

    float data[] = {1.0f, 2.0f, 3.0f};
    nimcp_future_complete(future, data, sizeof(data));

    // Try to get with smaller buffer
    float result[2];
    bool success = nimcp_future_get_result(future, result, sizeof(result));

    EXPECT_FALSE(success);  // Buffer too small

    nimcp_future_destroy(future, true);
}

//=============================================================================
// Callback Tests
//=============================================================================

static bool callback_invoked = false;
static float callback_result = 0.0f;

static void test_callback(void* user_data) {
    callback_invoked = true;
    if (user_data) {
        callback_result = *(float*)user_data;
    }
}

TEST_F(AsyncTest, CallbackOnCompletion) {
    callback_invoked = false;
    callback_result = 0.0f;

    nimcp_future_t future = nimcp_future_create();
    ASSERT_NE(future, nullptr);

    float data = 99.0f;
    nimcp_future_set_callback(future, test_callback, &data);

    // Callback should not be invoked yet
    EXPECT_FALSE(callback_invoked);

    // Complete the future
    nimcp_future_complete(future, &data, sizeof(float));

    // Give callback a moment to execute
    simulate_work_ms(10);

    EXPECT_TRUE(callback_invoked);
    EXPECT_FLOAT_EQ(callback_result, 99.0f);

    nimcp_future_destroy(future, true);
}

TEST_F(AsyncTest, CallbackOnError) {
    callback_invoked = false;

    nimcp_future_t future = nimcp_future_create();
    ASSERT_NE(future, nullptr);

    nimcp_future_set_callback(future, test_callback, nullptr);

    nimcp_future_set_error(future, "Test error");

    simulate_work_ms(10);

    EXPECT_TRUE(callback_invoked);

    nimcp_future_destroy(future, true);
}

TEST_F(AsyncTest, CallbackOnCancellation) {
    callback_invoked = false;

    nimcp_future_t future = nimcp_future_create();
    ASSERT_NE(future, nullptr);

    nimcp_future_set_callback(future, test_callback, nullptr);

    nimcp_future_cancel(future);

    simulate_work_ms(10);

    EXPECT_TRUE(callback_invoked);

    nimcp_future_destroy(future, true);
}

//=============================================================================
// Future Combinators Tests
//=============================================================================

TEST_F(AsyncTest, WaitAllEmpty) {
    nimcp_future_t* futures = nullptr;
    bool success = nimcp_future_wait_all(futures, 0, 1000);

    EXPECT_TRUE(success);  // Empty array should succeed immediately
}

TEST_F(AsyncTest, WaitAllSingle) {
    nimcp_future_t future = nimcp_future_create();
    ASSERT_NE(future, nullptr);

    float result = 42.0f;
    nimcp_future_complete(future, &result, sizeof(float));

    nimcp_future_t futures[] = {future};
    bool success = nimcp_future_wait_all(futures, 1, 1000);

    EXPECT_TRUE(success);

    nimcp_future_destroy(future, true);
}

TEST_F(AsyncTest, WaitAllMultiple) {
    nimcp_future_t f1 = nimcp_future_create();
    nimcp_future_t f2 = nimcp_future_create();
    nimcp_future_t f3 = nimcp_future_create();

    ASSERT_NE(f1, nullptr);
    ASSERT_NE(f2, nullptr);
    ASSERT_NE(f3, nullptr);

    // Complete in background threads with different delays
    std::thread t1([f1]() {
        simulate_work_ms(50);
        float r = 1.0f;
        nimcp_future_complete(f1, &r, sizeof(float));
    });

    std::thread t2([f2]() {
        simulate_work_ms(100);
        float r = 2.0f;
        nimcp_future_complete(f2, &r, sizeof(float));
    });

    std::thread t3([f3]() {
        simulate_work_ms(150);
        float r = 3.0f;
        nimcp_future_complete(f3, &r, sizeof(float));
    });

    nimcp_future_t futures[] = {f1, f2, f3};
    bool success = nimcp_future_wait_all(futures, 3, 500);  // 500ms timeout

    EXPECT_TRUE(success);
    EXPECT_TRUE(nimcp_future_is_done(f1));
    EXPECT_TRUE(nimcp_future_is_done(f2));
    EXPECT_TRUE(nimcp_future_is_done(f3));

    t1.join();
    t2.join();
    t3.join();

    nimcp_future_destroy(f1);
    nimcp_future_destroy(f2);
    nimcp_future_destroy(f3);
}

TEST_F(AsyncTest, WaitAllTimeout) {
    nimcp_future_t f1 = nimcp_future_create();
    nimcp_future_t f2 = nimcp_future_create();

    ASSERT_NE(f1, nullptr);
    ASSERT_NE(f2, nullptr);

    // Complete only one
    float r = 1.0f;
    nimcp_future_complete(f1, &r, sizeof(float));
    // f2 remains pending

    nimcp_future_t futures[] = {f1, f2};
    bool success = nimcp_future_wait_all(futures, 2, 100);

    EXPECT_FALSE(success);  // Should timeout
    EXPECT_TRUE(nimcp_future_is_done(f1));
    EXPECT_FALSE(nimcp_future_is_done(f2));

    nimcp_future_destroy(f1);
    nimcp_future_destroy(f2);
}

TEST_F(AsyncTest, WaitAnyEmpty) {
    nimcp_future_t* futures = nullptr;
    uint32_t index;
    bool success = nimcp_future_wait_any(futures, 0, &index, 1000);

    EXPECT_FALSE(success);  // Empty array should fail
}

TEST_F(AsyncTest, WaitAnySingle) {
    nimcp_future_t future = nimcp_future_create();
    ASSERT_NE(future, nullptr);

    float result = 42.0f;
    nimcp_future_complete(future, &result, sizeof(float));

    nimcp_future_t futures[] = {future};
    uint32_t index;
    bool success = nimcp_future_wait_any(futures, 1, &index, 1000);

    EXPECT_TRUE(success);
    EXPECT_EQ(index, 0);

    nimcp_future_destroy(future, true);
}

TEST_F(AsyncTest, WaitAnyFirstCompletes) {
    nimcp_future_t f1 = nimcp_future_create();
    nimcp_future_t f2 = nimcp_future_create();
    nimcp_future_t f3 = nimcp_future_create();

    ASSERT_NE(f1, nullptr);
    ASSERT_NE(f2, nullptr);
    ASSERT_NE(f3, nullptr);

    // Complete f2 first
    std::thread t2([f2]() {
        simulate_work_ms(50);
        float r = 2.0f;
        nimcp_future_complete(f2, &r, sizeof(float));
    });

    // f1 and f3 complete later
    std::thread t1([f1]() {
        simulate_work_ms(200);
        float r = 1.0f;
        nimcp_future_complete(f1, &r, sizeof(float));
    });

    std::thread t3([f3]() {
        simulate_work_ms(300);
        float r = 3.0f;
        nimcp_future_complete(f3, &r, sizeof(float));
    });

    nimcp_future_t futures[] = {f1, f2, f3};
    uint32_t index;
    bool success = nimcp_future_wait_any(futures, 3, &index, 1000);

    EXPECT_TRUE(success);
    EXPECT_EQ(index, 1);  // f2 completed first
    EXPECT_TRUE(nimcp_future_is_done(f2));

    t1.join();
    t2.join();
    t3.join();

    nimcp_future_destroy(f1);
    nimcp_future_destroy(f2);
    nimcp_future_destroy(f3);
}

TEST_F(AsyncTest, WaitAnyTimeout) {
    nimcp_future_t f1 = nimcp_future_create();
    nimcp_future_t f2 = nimcp_future_create();

    ASSERT_NE(f1, nullptr);
    ASSERT_NE(f2, nullptr);

    // Neither completes
    nimcp_future_t futures[] = {f1, f2};
    uint32_t index;
    bool success = nimcp_future_wait_any(futures, 2, &index, 100);

    EXPECT_FALSE(success);  // Should timeout

    nimcp_future_destroy(f1);
    nimcp_future_destroy(f2);
}

//=============================================================================
// Null Safety Tests
//=============================================================================

TEST_F(AsyncTest, NullFutureSafety) {
    EXPECT_EQ(nimcp_future_status(nullptr), NIMCP_FUTURE_PENDING);
    EXPECT_FALSE(nimcp_future_is_done(nullptr));
    EXPECT_FALSE(nimcp_future_wait(nullptr, 100));
    EXPECT_FALSE(nimcp_future_cancel(nullptr));

    float result;
    EXPECT_FALSE(nimcp_future_get_result(nullptr, &result, sizeof(float)));

    nimcp_future_set_callback(nullptr, test_callback, nullptr);
    nimcp_future_destroy(nullptr);

    SUCCEED();
}

TEST_F(AsyncTest, NullCombinatorsInputs) {
    nimcp_future_t future = nimcp_future_create();
    ASSERT_NE(future, nullptr);

    // Null futures array
    EXPECT_TRUE(nimcp_future_wait_all(nullptr, 0, 100));
    EXPECT_FALSE(nimcp_future_wait_all(nullptr, 1, 100));

    uint32_t index;
    EXPECT_FALSE(nimcp_future_wait_any(nullptr, 0, &index, 100));
    EXPECT_FALSE(nimcp_future_wait_any(nullptr, 1, &index, 100));

    // Null index pointer
    nimcp_future_t futures[] = {future};
    EXPECT_FALSE(nimcp_future_wait_any(futures, 1, nullptr, 100));

    nimcp_future_destroy(future, true);
}

//=============================================================================
// Thread Safety Tests
//=============================================================================

TEST_F(AsyncTest, ConcurrentStatusChecks) {
    nimcp_future_t future = nimcp_future_create();
    ASSERT_NE(future, nullptr);

    const int num_threads = 10;
    std::vector<std::thread> threads;

    // Multiple threads checking status concurrently
    for (int i = 0; i < num_threads; i++) {
        threads.emplace_back([future]() {
            for (int j = 0; j < 100; j++) {
                nimcp_future_status(future);
                nimcp_future_is_done(future);
            }
        });
    }

    // Complete while threads are checking
    simulate_work_ms(10);
    float result = 42.0f;
    nimcp_future_complete(future, &result, sizeof(float));

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(nimcp_future_status(future), NIMCP_FUTURE_COMPLETED);

    nimcp_future_destroy(future, true);
}

TEST_F(AsyncTest, ConcurrentWaits) {
    nimcp_future_t future = nimcp_future_create();
    ASSERT_NE(future, nullptr);

    const int num_waiters = 5;
    std::vector<std::thread> waiters;
    std::vector<bool> results(num_waiters);

    // Multiple threads waiting on same future
    for (int i = 0; i < num_waiters; i++) {
        waiters.emplace_back([future, &results, i]() {
            results[i] = nimcp_future_wait(future, 1000);
        });
    }

    // Complete after a delay
    simulate_work_ms(100);
    float result = 42.0f;
    nimcp_future_complete(future, &result, sizeof(float));

    for (auto& t : waiters) {
        t.join();
    }

    // All waiters should have succeeded
    for (int i = 0; i < num_waiters; i++) {
        EXPECT_TRUE(results[i]);
    }

    nimcp_future_destroy(future, true);
}
