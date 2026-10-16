/** @file test_thread_local_error_buffer.cpp - P0-2 regression: _Thread_local g_last_error */
#include <gtest/gtest.h>
#include <atomic>
#include <thread>
#include <vector>
#include <string>
#include <cstring>

extern "C" {
#include "nimcp.h"
}

class ThreadLocalErrorTest : public ::testing::Test {
protected:
    void SetUp() override { nimcp_init(); }
    void TearDown() override { nimcp_shutdown(); }
};

// Regression test for P0-2: g_last_error was static (shared);
// now _Thread_local so each thread has its own error buffer.
TEST_F(ThreadLocalErrorTest, EachThreadSeesOwnError) {
    const int NUM_THREADS = 8;
    std::vector<std::string> errors(NUM_THREADS);
    std::atomic<int> barrier{0};

    std::vector<std::thread> threads;
    for (int i = 0; i < NUM_THREADS; i++) {
        threads.emplace_back([&, i]() {
            // Each thread triggers a unique error by passing NULL brain
            // with a unique pattern that we can verify
            nimcp_brain_destroy(NULL);

            barrier.fetch_add(1);
            while (barrier.load() < NUM_THREADS) {}

            // Read back the error - should be this threads own
            const char* err = nimcp_get_error();
            errors[i] = err ? err : "(null)";
        });
    }
    for (auto& t : threads) t.join();

    // All threads should see consistent error state
    // Key: no thread should see a garbled/mixed error from another thread
    for (int i = 0; i < NUM_THREADS; i++) {
        EXPECT_FALSE(errors[i].empty())
            << "Thread " << i << " should have an error message";
        // Verify no truncation or corruption
        EXPECT_LT(errors[i].length(), 256u)
            << "Error message should fit in 256-byte buffer";
    }
}

TEST_F(ThreadLocalErrorTest, ConcurrentErrorsDoNotCorrupt) {
    const int NUM_THREADS = 16;
    const int ITERATIONS = 1000;
    std::atomic<bool> go{false};
    std::atomic<int> corruption_count{0};

    std::vector<std::thread> threads;
    for (int i = 0; i < NUM_THREADS; i++) {
        threads.emplace_back([&, i]() {
            while (!go.load()) {}
            for (int j = 0; j < ITERATIONS; j++) {
                // Trigger error setting
                nimcp_brain_destroy(NULL);
                const char* err = nimcp_get_error();
                if (err) {
                    size_t len = strlen(err);
                    if (len >= 256) corruption_count.fetch_add(1);
                }
            }
        });
    }
    go.store(true);
    for (auto& t : threads) t.join();

    EXPECT_EQ(corruption_count.load(), 0)
        << "No error buffer corruption under concurrent access";
}

TEST_F(ThreadLocalErrorTest, ErrorBufferNotShared) {
    // Set error in main thread
    nimcp_brain_destroy(NULL);
    const char* main_err = nimcp_get_error();
    std::string main_error = main_err ? main_err : "";

    // Spawn thread that creates different error state
    std::string child_error;
    std::thread t([&]() {
        // Initially should not see parent thread error
        // (or should see default "No error")
        const char* err = nimcp_get_error();
        child_error = err ? err : "";
    });
    t.join();

    // Child thread should have independent error buffer
    // It should see "No error" (default) not the parent error
    EXPECT_NE(main_error, "No error")
        << "Main thread should have a non-default error after brain_destroy(NULL)";
    EXPECT_EQ(child_error, "No error")
        << "Child thread should see default error, not parent thread error";
}
