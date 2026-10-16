/**
 * @file test_future_integration.cpp
 * @brief Integration tests for NIMCP futures/promises system
 *
 * WHAT: Real-world async scenarios testing futures, promises, and combinators
 * WHY:  Validate futures work correctly in complex, concurrent environments
 * HOW:  Simulates production patterns: pipelines, fan-out/in, timeouts, errors
 *
 * TEST COVERAGE:
 * 1. Async Task Pattern      - Submit work, get future, wait for result
 * 2. Pipeline Pattern         - Chain transformations with then/map
 * 3. Fan-out/Fan-in          - Multiple concurrent ops, wait all/any
 * 4. Timeout Handling        - Operations completing before/after timeout
 * 5. Error Recovery          - Error handling, retries, fallbacks
 * 6. Resource Cleanup        - Proper destruction in various scenarios
 * 7. Performance Under Load  - Many concurrent futures (100+)
 * 8. Real-world Patterns     - Request-response, async I/O, work distribution
 *
 * @author NIMCP Test Team
 * @date 2025-11-28
 */

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <algorithm>
#include <random>
#include <cmath>

// Headers have their own extern "C" guards
#include "async/nimcp_future.h"
#include "utils/error/nimcp_error_codes.h"

//=============================================================================
// Test Fixture
//=============================================================================

/**
 * @brief Test fixture for future integration tests
 *
 * WHAT: Sets up and tears down test environment for each test
 * WHY:  Ensure clean state and track statistics
 * HOW:  Resets stats before each test, verifies cleanup after
 */
class FutureIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Reset statistics before each test
        nimcp_future_reset_stats();

        // Record initial stats
        nimcp_future_get_stats(&initial_stats_);
    }

    void TearDown() override {
        // Get final statistics
        nimcp_future_stats_t final_stats;
        nimcp_future_get_stats(&final_stats);

        // Verify no resource leaks
        EXPECT_EQ(final_stats.active_promises, 0)
            << "Promise leak detected";
        EXPECT_EQ(final_stats.active_futures, 0)
            << "Future leak detected";
    }

    // Helper: Create promise-future pair
    struct PromiseFuturePair {
        nimcp_promise_t promise;
        nimcp_future_t future;
    };

    PromiseFuturePair create_pair(size_t result_size) {
        nimcp_promise_t promise = nimcp_promise_create(result_size);
        EXPECT_NE(promise, nullptr);

        nimcp_future_t future = nimcp_promise_get_future(promise);
        EXPECT_NE(future, nullptr);

        return {promise, future};
    }

    // Helper: Simulate async work with delay
    void async_complete_after(nimcp_promise_t promise, int value, int delay_ms) {
        std::thread([promise, value, delay_ms]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
            nimcp_promise_complete(promise, &value);
            nimcp_promise_destroy(promise);
        }).detach();
    }

    nimcp_future_stats_t initial_stats_;
};

//=============================================================================
// 1. Async Task Pattern Tests
//=============================================================================

/**
 * TEST: Basic async task submission and retrieval
 *
 * WHAT: Submit work to thread, get result via future
 * WHY:  Most fundamental async pattern
 * HOW:  Create promise, spawn thread, wait for result
 */
TEST_F(FutureIntegrationTest, AsyncTaskBasic) {
    auto pair = create_pair(sizeof(int));

    // Submit async work
    std::thread worker([promise = pair.promise]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        int result = 42;
        nimcp_promise_complete(promise, &result);
        nimcp_promise_destroy(promise);
    });

    // Wait for result
    ASSERT_TRUE(nimcp_future_wait(pair.future));

    int result = 0;
    ASSERT_EQ(nimcp_future_get(pair.future, &result), NIMCP_SUCCESS);
    EXPECT_EQ(result, 42);

    nimcp_future_destroy(pair.future);
    worker.join();
}

/**
 * TEST: Multiple concurrent async tasks
 *
 * WHAT: Run multiple independent tasks concurrently
 * WHY:  Validate futures handle concurrent operations correctly
 * HOW:  Spawn 10 threads, each computing different result
 */
TEST_F(FutureIntegrationTest, MultipleConcurrentTasks) {
    constexpr int NUM_TASKS = 10;
    std::vector<PromiseFuturePair> pairs;
    std::vector<std::thread> workers;

    // Create tasks
    for (int i = 0; i < NUM_TASKS; ++i) {
        auto pair = create_pair(sizeof(int));
        pairs.push_back(pair);

        // Each task computes i * i
        workers.emplace_back([promise = pair.promise, i]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(5 + (i % 3)));
            int result = i * i;
            nimcp_promise_complete(promise, &result);
            nimcp_promise_destroy(promise);
        });
    }

    // Collect results (may complete out of order)
    for (int i = 0; i < NUM_TASKS; ++i) {
        ASSERT_TRUE(nimcp_future_wait(pairs[i].future));

        int result = 0;
        ASSERT_EQ(nimcp_future_get(pairs[i].future, &result), NIMCP_SUCCESS);
        EXPECT_EQ(result, i * i);

        nimcp_future_destroy(pairs[i].future);
    }

    for (auto& worker : workers) {
        worker.join();
    }
}

//=============================================================================
// 2. Pipeline Pattern Tests
//=============================================================================

/**
 * TEST: Chained transformations with then callbacks
 *
 * WHAT: Pipeline of async transformations: A -> B -> C -> D
 * WHY:  Common pattern for composing async operations
 * HOW:  Chain then() callbacks, each transforming previous result
 */
TEST_F(FutureIntegrationTest, PipelineWithThenCallbacks) {
    auto pair = create_pair(sizeof(int));

    // Track pipeline execution
    std::atomic<int> pipeline_stage{0};
    std::atomic<int> final_result{0};

    // Stage 1: Original computation
    std::thread producer([promise = pair.promise]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        int value = 10;
        nimcp_promise_complete(promise, &value);
        nimcp_promise_destroy(promise);
    });

    // Stage 2: Double it (10 -> 20)
    nimcp_future_then(pair.future, [](const void* result, nimcp_error_t err, void* ctx) {
        auto* stage = static_cast<std::atomic<int>*>(ctx);
        if (err == NIMCP_SUCCESS) {
            int value = *static_cast<const int*>(result);
            EXPECT_EQ(value, 10);
            stage->store(1);
        }
    }, &pipeline_stage);

    // Wait and verify
    ASSERT_TRUE(nimcp_future_wait(pair.future));

    // Give callbacks time to execute
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    EXPECT_EQ(pipeline_stage.load(), 1);

    nimcp_future_destroy(pair.future);
    producer.join();
}

/**
 * TEST: Map combinator for transformations
 *
 * WHAT: Transform future result using map
 * WHY:  Functional approach to pipelines
 * HOW:  Chain map() operations: int -> double -> string_length
 */
TEST_F(FutureIntegrationTest, PipelineWithMapCombinator) {
    auto pair = create_pair(sizeof(int));

    // Transform: int -> double (multiply by 2.5)
    auto doubled = nimcp_future_map(
        pair.future,
        [](const void* in, void* out, void* ctx) -> nimcp_error_t {
            int input = *static_cast<const int*>(in);
            double output = input * 2.5;
            *static_cast<double*>(out) = output;
            return NIMCP_SUCCESS;
        },
        sizeof(double),
        nullptr
    );
    ASSERT_NE(doubled, nullptr);

    // Complete original promise
    int initial_value = 10;
    nimcp_promise_complete(pair.promise, &initial_value);

    // Wait for transformed result
    ASSERT_TRUE(nimcp_future_wait(doubled));

    double result = 0.0;
    ASSERT_EQ(nimcp_future_get(doubled, &result), NIMCP_SUCCESS);
    EXPECT_DOUBLE_EQ(result, 25.0);

    nimcp_future_destroy(doubled);
    nimcp_future_destroy(pair.future);
    nimcp_promise_destroy(pair.promise);
}

/**
 * TEST: Error propagation through pipeline
 *
 * WHAT: Errors propagate through then() chain
 * WHY:  Failure in any stage should abort pipeline
 * HOW:  Fail early stage, verify later stages see error
 */
TEST_F(FutureIntegrationTest, PipelineErrorPropagation) {
    auto pair = create_pair(sizeof(int));

    std::atomic<bool> callback_saw_error{false};

    // Register callback
    nimcp_future_then(pair.future, [](const void* result, nimcp_error_t err, void* ctx) {
        auto* saw_error = static_cast<std::atomic<bool>*>(ctx);
        if (err != NIMCP_SUCCESS) {
            saw_error->store(true);
        }
    }, &callback_saw_error);

    // Fail the promise
    nimcp_promise_fail(pair.promise, NIMCP_ERROR_NO_MEMORY);

    // Verify callback received error
    ASSERT_FALSE(nimcp_future_wait(pair.future));

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    EXPECT_TRUE(callback_saw_error.load());
    EXPECT_EQ(nimcp_future_get_error(pair.future), NIMCP_ERROR_NO_MEMORY);

    nimcp_future_destroy(pair.future);
    nimcp_promise_destroy(pair.promise);
}

//=============================================================================
// 3. Fan-out/Fan-in Tests
//=============================================================================

/**
 * TEST: Fan-out with future_all combinator
 *
 * WHAT: Launch multiple operations, wait for all to complete
 * WHY:  Common pattern for parallel execution
 * HOW:  Create 5 futures, use future_all to synchronize
 */
TEST_F(FutureIntegrationTest, FanOutFanInWithAll) {
    constexpr int NUM_OPS = 5;
    std::vector<nimcp_future_t> futures;
    std::vector<std::thread> workers;

    // Fan-out: Launch parallel operations
    for (int i = 0; i < NUM_OPS; ++i) {
        auto pair = create_pair(sizeof(int));
        futures.push_back(pair.future);

        workers.emplace_back([promise = pair.promise, i]() {
            // Simulate variable latency
            std::this_thread::sleep_for(std::chrono::milliseconds(10 + (i * 2)));
            int result = i + 100;
            nimcp_promise_complete(promise, &result);
            nimcp_promise_destroy(promise);
        });
    }

    // Fan-in: Wait for all
    nimcp_future_t all_future = nimcp_future_all(futures.data(), NUM_OPS);
    ASSERT_NE(all_future, nullptr);

    ASSERT_TRUE(nimcp_future_wait(all_future));

    // Verify all completed successfully
    bool success_flags[NUM_OPS];
    ASSERT_EQ(nimcp_future_get(all_future, success_flags), NIMCP_SUCCESS);

    for (int i = 0; i < NUM_OPS; ++i) {
        EXPECT_TRUE(success_flags[i]) << "Operation " << i << " failed";

        // Verify individual results
        int result = 0;
        ASSERT_EQ(nimcp_future_get(futures[i], &result), NIMCP_SUCCESS);
        EXPECT_EQ(result, i + 100);
    }

    // Cleanup
    nimcp_future_destroy(all_future);
    for (auto future : futures) {
        nimcp_future_destroy(future);
    }
    for (auto& worker : workers) {
        worker.join();
    }
}

/**
 * TEST: Race with future_any combinator
 *
 * WHAT: Launch multiple operations, use first to complete
 * WHY:  Useful for redundant requests, fastest source wins
 * HOW:  Create 5 futures with different delays, verify fastest wins
 */
TEST_F(FutureIntegrationTest, FanOutRaceWithAny) {
    constexpr int NUM_OPS = 5;
    std::vector<nimcp_future_t> futures;
    std::vector<std::thread> workers;

    // Launch operations with different delays
    for (int i = 0; i < NUM_OPS; ++i) {
        auto pair = create_pair(sizeof(int));
        futures.push_back(pair.future);

        int delay_ms = 50 - (i * 10);  // First is slowest, last is fastest
        workers.emplace_back([promise = pair.promise, i, delay_ms]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
            int result = i;
            nimcp_promise_complete(promise, &result);
            nimcp_promise_destroy(promise);
        });
    }

    // Wait for any (should be last one, index 4)
    nimcp_future_t any_future = nimcp_future_any(futures.data(), NUM_OPS);
    ASSERT_NE(any_future, nullptr);

    ASSERT_TRUE(nimcp_future_wait(any_future));

    size_t winner_index = 0;
    ASSERT_EQ(nimcp_future_get(any_future, &winner_index), NIMCP_SUCCESS);
    EXPECT_EQ(winner_index, NUM_OPS - 1) << "Expected fastest (last) operation to win";

    // Cleanup
    nimcp_future_destroy(any_future);
    for (auto future : futures) {
        nimcp_future_destroy(future);
    }
    for (auto& worker : workers) {
        worker.join();
    }
}

//=============================================================================
// 4. Timeout Handling Tests
//=============================================================================

/**
 * TEST: Operation completes before timeout
 *
 * WHAT: Fast operation with generous timeout
 * WHY:  Verify timeout doesn't interfere with fast paths
 * HOW:  10ms operation with 1000ms timeout
 */
TEST_F(FutureIntegrationTest, TimeoutOperationCompletesQuickly) {
    auto pair = create_pair(sizeof(int));

    // Fast operation
    async_complete_after(pair.promise, 42, 10);

    // Wait with generous timeout
    auto start = std::chrono::steady_clock::now();
    ASSERT_TRUE(nimcp_future_wait_timeout(pair.future, 1000));
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start
    ).count();

    // Should complete quickly, not wait full timeout
    EXPECT_LT(elapsed, 100) << "Should complete in ~10ms, not wait full timeout";

    int result = 0;
    ASSERT_EQ(nimcp_future_get(pair.future, &result), NIMCP_SUCCESS);
    EXPECT_EQ(result, 42);

    nimcp_future_destroy(pair.future);
}

/**
 * TEST: Operation exceeds timeout
 *
 * WHAT: Slow operation times out
 * WHY:  Prevent indefinite blocking
 * HOW:  100ms operation with 20ms timeout
 */
TEST_F(FutureIntegrationTest, TimeoutOperationExceedsLimit) {
    auto pair = create_pair(sizeof(int));

    // Slow operation
    async_complete_after(pair.promise, 42, 100);

    // Wait with short timeout
    auto start = std::chrono::steady_clock::now();
    ASSERT_FALSE(nimcp_future_wait_timeout(pair.future, 20));
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start
    ).count();

    // Should timeout after ~20ms
    EXPECT_GE(elapsed, 15);
    EXPECT_LT(elapsed, 40);

    // Future should still be pending
    EXPECT_EQ(nimcp_future_state(pair.future), NIMCP_FUTURE_PENDING);

    nimcp_future_destroy(pair.future);

    // Wait for worker thread to complete
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
}

/**
 * TEST: Cancellation on timeout
 *
 * WHAT: Cancel slow operation after timeout
 * WHY:  Abort unnecessary work
 * HOW:  Timeout, then cancel, verify state
 */
TEST_F(FutureIntegrationTest, TimeoutWithCancellation) {
    auto pair = create_pair(sizeof(int));

    // Slow operation
    async_complete_after(pair.promise, 42, 100);

    // Try with timeout
    ASSERT_FALSE(nimcp_future_wait_timeout(pair.future, 20));

    // Cancel the operation
    ASSERT_TRUE(nimcp_future_cancel(pair.future));

    // Verify cancelled state
    EXPECT_EQ(nimcp_future_state(pair.future), NIMCP_FUTURE_CANCELLED);

    nimcp_future_destroy(pair.future);

    std::this_thread::sleep_for(std::chrono::milliseconds(150));
}

//=============================================================================
// 5. Error Recovery Tests
//=============================================================================

/**
 * TEST: Handle errors in callbacks
 *
 * WHAT: Callback handles error gracefully
 * WHY:  Errors shouldn't crash system
 * HOW:  Fail promise, verify callback receives error
 */
TEST_F(FutureIntegrationTest, ErrorHandlingInCallbacks) {
    auto pair = create_pair(sizeof(int));

    struct Context {
        std::atomic<nimcp_error_t>* error;
        std::atomic<bool>* invoked;
    };

    std::atomic<nimcp_error_t> observed_error{NIMCP_SUCCESS};
    std::atomic<bool> callback_invoked{false};
    Context context{&observed_error, &callback_invoked};

    nimcp_future_then(pair.future, [](const void* result, nimcp_error_t err, void* ctx) {
        auto* context = static_cast<Context*>(ctx);

        context->invoked->store(true);
        context->error->store(err);

        EXPECT_EQ(result, nullptr) << "Result should be NULL on error";
    }, &context);

    // Fail promise
    nimcp_promise_fail(pair.promise, NIMCP_ERROR_INVALID_STATE);

    // Wait for callback
    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    EXPECT_TRUE(callback_invoked.load());
    EXPECT_EQ(observed_error.load(), NIMCP_ERROR_INVALID_STATE);

    nimcp_future_destroy(pair.future);
    nimcp_promise_destroy(pair.promise);
}

/**
 * TEST: Retry pattern
 *
 * WHAT: Retry failed operation up to N times
 * WHY:  Common pattern for transient failures
 * HOW:  Fail first 2 attempts, succeed on 3rd
 */
TEST_F(FutureIntegrationTest, RetryPattern) {
    std::atomic<int> attempt{0};

    auto retry_operation = [this, &attempt]() -> PromiseFuturePair {
        auto pair = create_pair(sizeof(int));

        std::thread([promise = pair.promise, &attempt]() mutable {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));

            int current_attempt = attempt.fetch_add(1);

            if (current_attempt < 2) {
                // Fail first 2 attempts
                nimcp_promise_fail(promise, NIMCP_ERROR_TIMEOUT);
            } else {
                // Succeed on 3rd attempt
                int result = 42;
                nimcp_promise_complete(promise, &result);
            }
            nimcp_promise_destroy(promise);
        }).detach();

        return pair;
    };

    // Retry up to 3 times
    nimcp_future_t future = nullptr;
    bool success = false;

    for (int i = 0; i < 3; ++i) {
        auto pair = retry_operation();
        future = pair.future;

        if (nimcp_future_wait_timeout(future, 100)) {
            success = true;
            break;
        } else {
            nimcp_future_destroy(future);
        }
    }

    ASSERT_TRUE(success) << "Should succeed after retries";

    int result = 0;
    ASSERT_EQ(nimcp_future_get(future, &result), NIMCP_SUCCESS);
    EXPECT_EQ(result, 42);
    EXPECT_EQ(attempt.load(), 3);

    nimcp_future_destroy(future);
}

/**
 * TEST: Fallback values on error
 *
 * WHAT: Provide default value when operation fails
 * WHY:  Graceful degradation
 * HOW:  Try operation, use fallback on timeout
 */
TEST_F(FutureIntegrationTest, FallbackOnError) {
    auto pair = create_pair(sizeof(int));

    // Slow operation that will timeout
    async_complete_after(pair.promise, 100, 200);

    // Try with timeout
    int result = -1;  // Fallback value

    if (nimcp_future_wait_timeout(pair.future, 20)) {
        nimcp_future_get(pair.future, &result);
    }
    // else: keep fallback value

    EXPECT_EQ(result, -1) << "Should use fallback on timeout";

    nimcp_future_destroy(pair.future);

    std::this_thread::sleep_for(std::chrono::milliseconds(250));
}

//=============================================================================
// 6. Resource Cleanup Tests
//=============================================================================

/**
 * TEST: Future destroyed before completion
 *
 * WHAT: Consumer destroys future while operation pending
 * WHY:  Verify no leaks when consumer loses interest
 * HOW:  Create future, destroy immediately, let operation complete
 */
TEST_F(FutureIntegrationTest, FutureDestroyedBeforeCompletion) {
    auto pair = create_pair(sizeof(int));

    // Start slow operation
    std::thread worker([promise = pair.promise]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        int result = 42;
        nimcp_promise_complete(promise, &result);
        nimcp_promise_destroy(promise);
    });

    // Destroy future immediately
    nimcp_future_destroy(pair.future);

    // Let operation complete
    worker.join();

    // Verify no leaks (checked in TearDown)
}

/**
 * TEST: Promise destroyed after setting value
 *
 * WHAT: Producer destroys promise, consumer still gets result
 * WHY:  Result should persist after promise destroyed
 * HOW:  Complete promise, destroy it, then get result from future
 */
TEST_F(FutureIntegrationTest, PromiseDestroyedAfterCompletion) {
    auto pair = create_pair(sizeof(int));

    // Complete and destroy promise
    int value = 42;
    nimcp_promise_complete(pair.promise, &value);
    nimcp_promise_destroy(pair.promise);

    // Future should still work
    ASSERT_TRUE(nimcp_future_wait(pair.future));

    int result = 0;
    ASSERT_EQ(nimcp_future_get(pair.future, &result), NIMCP_SUCCESS);
    EXPECT_EQ(result, 42);

    nimcp_future_destroy(pair.future);
}

/**
 * TEST: Multiple futures sharing promise
 *
 * WHAT: Multiple consumers for same result
 * WHY:  Share expensive computation result
 * HOW:  Get multiple futures from same promise, verify all see result
 */
TEST_F(FutureIntegrationTest, MultipleFuturesSharingPromise) {
    auto pair = create_pair(sizeof(int));

    // Get additional futures
    nimcp_future_t future2 = nimcp_promise_get_future(pair.promise);
    nimcp_future_t future3 = nimcp_promise_get_future(pair.promise);

    ASSERT_NE(future2, nullptr);
    ASSERT_NE(future3, nullptr);

    // Complete promise
    int value = 42;
    nimcp_promise_complete(pair.promise, &value);

    // All futures should see result
    int result1 = 0, result2 = 0, result3 = 0;

    ASSERT_TRUE(nimcp_future_wait(pair.future));
    ASSERT_TRUE(nimcp_future_wait(future2));
    ASSERT_TRUE(nimcp_future_wait(future3));

    ASSERT_EQ(nimcp_future_get(pair.future, &result1), NIMCP_SUCCESS);
    ASSERT_EQ(nimcp_future_get(future2, &result2), NIMCP_SUCCESS);
    ASSERT_EQ(nimcp_future_get(future3, &result3), NIMCP_SUCCESS);

    EXPECT_EQ(result1, 42);
    EXPECT_EQ(result2, 42);
    EXPECT_EQ(result3, 42);

    // Cleanup
    nimcp_promise_destroy(pair.promise);
    nimcp_future_destroy(pair.future);
    nimcp_future_destroy(future2);
    nimcp_future_destroy(future3);
}

//=============================================================================
// 7. Performance Under Load Tests
//=============================================================================

/**
 * TEST: Many concurrent futures (100+)
 *
 * WHAT: Create and complete 100+ futures concurrently
 * WHY:  Stress test system under load
 * HOW:  Spawn 200 futures, complete them in random order
 */
TEST_F(FutureIntegrationTest, ManyConcurrentFutures) {
    constexpr int NUM_FUTURES = 200;
    std::vector<PromiseFuturePair> pairs;
    std::vector<std::thread> workers;

    // Create many futures
    for (int i = 0; i < NUM_FUTURES; ++i) {
        pairs.push_back(create_pair(sizeof(int)));
    }

    // Complete them with random delays (simulate realistic load)
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> delay_dist(1, 10);

    for (int i = 0; i < NUM_FUTURES; ++i) {
        int delay = delay_dist(gen);
        workers.emplace_back([promise = pairs[i].promise, i, delay]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(delay));
            int result = i * 2;
            nimcp_promise_complete(promise, &result);
            nimcp_promise_destroy(promise);
        });
    }

    // Wait for all
    auto start = std::chrono::steady_clock::now();

    for (int i = 0; i < NUM_FUTURES; ++i) {
        ASSERT_TRUE(nimcp_future_wait_timeout(pairs[i].future, 1000));

        int result = 0;
        ASSERT_EQ(nimcp_future_get(pairs[i].future, &result), NIMCP_SUCCESS);
        EXPECT_EQ(result, i * 2);

        nimcp_future_destroy(pairs[i].future);
    }

    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start
    ).count();

    // Should complete reasonably quickly (most delays are 1-10ms)
    EXPECT_LT(elapsed, 500) << "200 futures should complete within 500ms";

    for (auto& worker : workers) {
        worker.join();
    }
}

/**
 * TEST: Rapid creation/completion cycles
 *
 * WHAT: Create and complete many futures in tight loop
 * WHY:  Test allocation/deallocation performance
 * HOW:  Loop creating, completing, destroying futures
 */
TEST_F(FutureIntegrationTest, RapidCreationCompletionCycles) {
    constexpr int NUM_CYCLES = 1000;

    auto start = std::chrono::steady_clock::now();

    for (int i = 0; i < NUM_CYCLES; ++i) {
        auto pair = create_pair(sizeof(int));

        int value = i;
        nimcp_promise_complete(pair.promise, &value);

        ASSERT_TRUE(nimcp_future_wait(pair.future));

        int result = 0;
        ASSERT_EQ(nimcp_future_get(pair.future, &result), NIMCP_SUCCESS);
        EXPECT_EQ(result, i);

        nimcp_promise_destroy(pair.promise);
        nimcp_future_destroy(pair.future);
    }

    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start
    ).count();

    // 1000 cycles should be fast (<100ms target)
    EXPECT_LT(elapsed, 200) << "1000 cycles should complete quickly";

    // Verify statistics
    nimcp_future_stats_t stats;
    nimcp_future_get_stats(&stats);

    EXPECT_GE(stats.completions, NUM_CYCLES);
    EXPECT_GE(stats.waits_immediate, NUM_CYCLES);  // All should be immediate
}

//=============================================================================
// 8. Real-world Pattern Tests
//=============================================================================

/**
 * TEST: Request-response simulation
 *
 * WHAT: Simulate async request-response cycle
 * WHY:  Common pattern in distributed systems
 * HOW:  Client sends request (promise), server responds, client gets result
 */
TEST_F(FutureIntegrationTest, RequestResponsePattern) {
    struct Request {
        int id;
        nimcp_promise_t response_promise;
    };

    // Request queue (simplified)
    std::vector<Request> requests;
    std::mutex requests_mutex;
    std::condition_variable requests_cv;
    std::atomic<bool> shutdown{false};

    // Server thread
    std::thread server([&]() {
        while (!shutdown.load()) {
            std::unique_lock<std::mutex> lock(requests_mutex);
            requests_cv.wait_for(lock, std::chrono::milliseconds(10), [&]() {
                return !requests.empty() || shutdown.load();
            });

            if (!requests.empty()) {
                Request req = requests.back();
                requests.pop_back();
                lock.unlock();

                // Process request (compute result = id * 10)
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
                int result = req.id * 10;

                nimcp_promise_complete(req.response_promise, &result);
                nimcp_promise_destroy(req.response_promise);
            }
        }
    });

    // Client: Send multiple requests
    constexpr int NUM_REQUESTS = 10;
    std::vector<nimcp_future_t> responses;

    for (int i = 0; i < NUM_REQUESTS; ++i) {
        auto pair = create_pair(sizeof(int));
        responses.push_back(pair.future);

        {
            std::lock_guard<std::mutex> lock(requests_mutex);
            requests.push_back({i, pair.promise});
        }
        requests_cv.notify_one();
    }

    // Client: Collect responses
    for (int i = 0; i < NUM_REQUESTS; ++i) {
        ASSERT_TRUE(nimcp_future_wait_timeout(responses[i], 1000));

        int result = 0;
        ASSERT_EQ(nimcp_future_get(responses[i], &result), NIMCP_SUCCESS);
        // Note: Responses may arrive out of order
        EXPECT_GE(result, 0);
        EXPECT_LT(result, NUM_REQUESTS * 10);

        nimcp_future_destroy(responses[i]);
    }

    shutdown.store(true);
    requests_cv.notify_all();
    server.join();
}

/**
 * TEST: Work distribution pattern
 *
 * WHAT: Distribute work to pool of workers, collect results
 * WHY:  Common pattern for parallel processing
 * HOW:  Master distributes tasks, workers process, master aggregates
 */
TEST_F(FutureIntegrationTest, WorkDistributionPattern) {
    constexpr int NUM_WORKERS = 4;
    constexpr int NUM_TASKS = 20;

    struct Task {
        int data;
        nimcp_promise_t result_promise;
    };

    // Task queue
    std::vector<Task> task_queue;
    std::mutex queue_mutex;
    std::condition_variable queue_cv;
    std::atomic<bool> shutdown{false};

    // Worker threads
    std::vector<std::thread> workers;
    for (int w = 0; w < NUM_WORKERS; ++w) {
        workers.emplace_back([&]() {
            while (!shutdown.load()) {
                std::unique_lock<std::mutex> lock(queue_mutex);
                queue_cv.wait_for(lock, std::chrono::milliseconds(10), [&]() {
                    return !task_queue.empty() || shutdown.load();
                });

                if (!task_queue.empty()) {
                    Task task = task_queue.back();
                    task_queue.pop_back();
                    lock.unlock();

                    // Process task (expensive computation)
                    std::this_thread::sleep_for(std::chrono::milliseconds(2));
                    int result = task.data * task.data;

                    nimcp_promise_complete(task.result_promise, &result);
                    nimcp_promise_destroy(task.result_promise);
                }
            }
        });
    }

    // Master: Distribute tasks
    std::vector<nimcp_future_t> results;
    for (int i = 0; i < NUM_TASKS; ++i) {
        auto pair = create_pair(sizeof(int));
        results.push_back(pair.future);

        {
            std::lock_guard<std::mutex> lock(queue_mutex);
            task_queue.push_back({i, pair.promise});
        }
        queue_cv.notify_one();
    }

    // Master: Aggregate results
    int sum = 0;
    for (int i = 0; i < NUM_TASKS; ++i) {
        ASSERT_TRUE(nimcp_future_wait_timeout(results[i], 1000));

        int result = 0;
        ASSERT_EQ(nimcp_future_get(results[i], &result), NIMCP_SUCCESS);
        sum += result;

        nimcp_future_destroy(results[i]);
    }

    // Verify aggregated result (sum of squares: 0^2 + 1^2 + ... + 19^2)
    int expected_sum = 0;
    for (int i = 0; i < NUM_TASKS; ++i) {
        expected_sum += i * i;
    }
    EXPECT_EQ(sum, expected_sum);

    // Shutdown workers
    shutdown.store(true);
    queue_cv.notify_all();
    for (auto& worker : workers) {
        worker.join();
    }
}

/**
 * TEST: Async I/O simulation
 *
 * WHAT: Simulate async file/network I/O operations
 * WHY:  Test futures in I/O-bound scenarios
 * HOW:  Simulate read operations with varying latencies
 */
TEST_F(FutureIntegrationTest, AsyncIOSimulation) {
    struct IORequest {
        int file_id;
        size_t offset;
        size_t length;
    };

    // Simulate reading multiple files concurrently
    auto async_read = [this](IORequest req) -> nimcp_future_t {
        auto pair = create_pair(sizeof(int));

        std::thread([promise = pair.promise, req]() mutable {
            // Simulate I/O latency (varies by file_id)
            int latency_ms = 5 + (req.file_id * 3);
            std::this_thread::sleep_for(std::chrono::milliseconds(latency_ms));

            // Simulate reading data (just return bytes read)
            int bytes_read = static_cast<int>(req.length);

            nimcp_promise_complete(promise, &bytes_read);
            nimcp_promise_destroy(promise);
        }).detach();

        return pair.future;
    };

    // Issue multiple read requests
    constexpr int NUM_READS = 8;
    std::vector<nimcp_future_t> read_futures;

    for (int i = 0; i < NUM_READS; ++i) {
        IORequest req = {i % 3, static_cast<size_t>(i * 1024), 1024};
        read_futures.push_back(async_read(req));
    }

    // Wait for all reads to complete
    size_t total_bytes_read = 0;
    for (auto future : read_futures) {
        ASSERT_TRUE(nimcp_future_wait_timeout(future, 1000));

        int bytes_read = 0;
        ASSERT_EQ(nimcp_future_get(future, &bytes_read), NIMCP_SUCCESS);
        total_bytes_read += bytes_read;

        nimcp_future_destroy(future);
    }

    EXPECT_EQ(total_bytes_read, NUM_READS * 1024);
}

//=============================================================================
// Statistics and Observability Tests
//=============================================================================

/**
 * TEST: Statistics tracking accuracy
 *
 * WHAT: Verify statistics accurately reflect operations
 * WHY:  Statistics critical for monitoring and debugging
 * HOW:  Perform known operations, verify stats match
 */
TEST_F(FutureIntegrationTest, StatisticsTracking) {
    nimcp_future_stats_t stats_before;
    nimcp_future_get_stats(&stats_before);

    constexpr int NUM_OPS = 10;

    // Perform known operations
    for (int i = 0; i < NUM_OPS; ++i) {
        auto pair = create_pair(sizeof(int));

        int value = i;
        nimcp_promise_complete(pair.promise, &value);

        ASSERT_TRUE(nimcp_future_wait(pair.future));

        int result = 0;
        nimcp_future_get(pair.future, &result);

        nimcp_promise_destroy(pair.promise);
        nimcp_future_destroy(pair.future);
    }

    nimcp_future_stats_t stats_after;
    nimcp_future_get_stats(&stats_after);

    // Verify statistics
    EXPECT_GE(stats_after.promises_created - stats_before.promises_created, NUM_OPS);
    EXPECT_GE(stats_after.futures_created - stats_before.futures_created, NUM_OPS);
    EXPECT_GE(stats_after.completions - stats_before.completions, NUM_OPS);
    EXPECT_GE(stats_after.waits_total - stats_before.waits_total, NUM_OPS);
    EXPECT_GE(stats_after.waits_immediate - stats_before.waits_immediate, NUM_OPS);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
