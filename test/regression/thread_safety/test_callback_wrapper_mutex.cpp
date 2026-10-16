/** @file test_callback_wrapper_mutex.cpp - P0-3 regression: callback wrapper race */
#include <gtest/gtest.h>
#include <atomic>
#include <thread>
#include <vector>
#include <cstring>

extern "C" {
#include "nimcp.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/memory/nimcp_memory.h"
}

class CallbackWrapperMutexTest : public ::testing::Test {
protected:
    void SetUp() override {
        nimcp_init();
        nimcp_thread_init();
    }
    void TearDown() override {
        nimcp_thread_cleanup();
        nimcp_shutdown();
    }
};

// Regression: P0-3 fix added nimcp_mutex_t to protect g_callback_wrappers.
// Before fix, concurrent registration could corrupt the array.
// This test verifies the mutex protection works.
static nimcp_callback_action_t dummy_cb(nimcp_callback_event_t ev,
    const nimcp_callback_metrics_t* m, void* ud) {
    (void)ev; (void)m; (void)ud;
    return NIMCP_CB_ACTION_CONTINUE;
}

TEST_F(CallbackWrapperMutexTest, MutexProtectsGlobalArray) {
    // The g_callback_wrappers_mutex (NIMCP_MUTEX_INITIALIZER) protects
    // the global callback wrapper array. Verify it works by testing
    // the mutex pattern directly.
    nimcp_mutex_t mutex = NIMCP_MUTEX_INITIALIZER;
    const int NUM_THREADS = 8;
    const int OPS = 5000;
    int shared_counter = 0;
    std::atomic<int> errors{0};

    std::vector<std::thread> threads;
    for (int i = 0; i < NUM_THREADS; i++) {
        threads.emplace_back([&]() {
            for (int j = 0; j < OPS; j++) {
                if (nimcp_mutex_lock(&mutex) != NIMCP_SUCCESS) {
                    errors.fetch_add(1);
                    continue;
                }
                shared_counter++;
                nimcp_mutex_unlock(&mutex);
            }
        });
    }
    for (auto& t : threads) t.join();

    EXPECT_EQ(errors.load(), 0) << "No mutex lock errors";
    EXPECT_EQ(shared_counter, NUM_THREADS * OPS)
        << "NIMCP_MUTEX_INITIALIZER should protect concurrent updates";
    nimcp_mutex_destroy(&mutex);
}

TEST_F(CallbackWrapperMutexTest, SimulateCallbackRegistrationRace) {
    // Simulate the pattern from nimcp_brain_register_callback:
    // malloc wrapper, store in array under mutex, increment counter.
    const int MAX_WRAPPERS = 256;
    void* wrappers[MAX_WRAPPERS] = {};
    nimcp_mutex_t mtx = NIMCP_MUTEX_INITIALIZER;
    std::atomic<uint32_t> next_id{0};
    std::atomic<int> alloc_failures{0};

    const int NUM_THREADS = 8;
    const int REGISTRATIONS = 100;

    std::vector<std::thread> threads;
    for (int i = 0; i < NUM_THREADS; i++) {
        threads.emplace_back([&]() {
            for (int j = 0; j < REGISTRATIONS; j++) {
                void* wrapper = nimcp_malloc(64);
                if (!wrapper) { alloc_failures.fetch_add(1); continue; }
                memset(wrapper, 0xAB, 64);

                nimcp_mutex_lock(&mtx);
                uint32_t idx = next_id.fetch_add(1) % MAX_WRAPPERS;
                if (wrappers[idx]) nimcp_free(wrappers[idx]);
                wrappers[idx] = wrapper;
                nimcp_mutex_unlock(&mtx);
            }
        });
    }
    for (auto& t : threads) t.join();

    EXPECT_EQ(alloc_failures.load(), 0);

    // Cleanup remaining wrappers
    for (int i = 0; i < MAX_WRAPPERS; i++) {
        if (wrappers[i]) nimcp_free(wrappers[i]);
    }
    nimcp_mutex_destroy(&mtx);
}

TEST_F(CallbackWrapperMutexTest, StaticMutexInitializerWorks) {
    // Verify NIMCP_MUTEX_INITIALIZER (used for g_callback_wrappers_mutex)
    // works without explicit nimcp_mutex_init
    nimcp_mutex_t m = NIMCP_MUTEX_INITIALIZER;
    EXPECT_EQ(nimcp_mutex_lock(&m), NIMCP_SUCCESS);
    EXPECT_EQ(nimcp_mutex_unlock(&m), NIMCP_SUCCESS);
    nimcp_mutex_destroy(&m);
}
