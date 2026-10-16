/** @file test_concurrent_module_init.cpp - Integration: concurrent init with nimcp_once */
#include <gtest/gtest.h>
#include <atomic>
#include <thread>
#include <vector>
#include <chrono>

extern "C" {
#include "utils/thread/nimcp_thread.h"
#include "utils/memory/nimcp_memory.h"
#include "nimcp.h"
}

class ConcurrentModuleInitTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// Test nimcp_once pattern for module initialization
static std::atomic<int> g_init_count{0};
static nimcp_once_t g_test_once = NIMCP_ONCE_INIT;
static void test_module_init(void) {
    g_init_count.fetch_add(1);
    // Simulate some initialization work
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
}

TEST_F(ConcurrentModuleInitTest, OncePatternSingleInit) {
    g_init_count.store(0);
    const int NUM_THREADS = 16;
    std::atomic<bool> go{false};
    std::vector<nimcp_result_t> results(NUM_THREADS);

    std::vector<std::thread> threads;
    for (int i = 0; i < NUM_THREADS; i++) {
        threads.emplace_back([&, i]() {
            while (!go.load()) {}
            results[i] = nimcp_once(&g_test_once, test_module_init);
        });
    }
    go.store(true);
    for (auto& t : threads) t.join();

    EXPECT_EQ(g_init_count.load(), 1)
        << "nimcp_once should init module exactly once under contention";
    for (int i = 0; i < NUM_THREADS; i++) {
        EXPECT_EQ(results[i], NIMCP_SUCCESS)
            << "All threads should get SUCCESS from nimcp_once";
    }
}

TEST_F(ConcurrentModuleInitTest, MutexProtectedGlobalsUnderContention) {
    nimcp_thread_init();
    nimcp_mutex_t mutex;
    mutex_attr_t attr; attr.type = MUTEX_TYPE_NORMAL;
    nimcp_mutex_init(&mutex, &attr);

    int shared_state = 0;
    const int NUM_THREADS = 8;
    const int OPS = 10000;
    std::atomic<int> errors{0};

    std::vector<std::thread> threads;
    for (int i = 0; i < NUM_THREADS; i++) {
        threads.emplace_back([&]() {
            for (int j = 0; j < OPS; j++) {
                nimcp_result_t r = nimcp_mutex_lock(&mutex);
                if (r != NIMCP_SUCCESS) { errors.fetch_add(1); continue; }
                shared_state++;
                nimcp_mutex_unlock(&mutex);
            }
        });
    }
    for (auto& t : threads) t.join();

    EXPECT_EQ(errors.load(), 0);
    EXPECT_EQ(shared_state, NUM_THREADS * OPS)
        << "Mutex-protected global should be consistent under contention";
    nimcp_mutex_destroy(&mutex);
    nimcp_thread_cleanup();
}

TEST_F(ConcurrentModuleInitTest, ThreadSubsystemInit) {
    // Verify nimcp_thread_init / cleanup can be called safely
    nimcp_result_t r = nimcp_thread_init();
    EXPECT_EQ(r, NIMCP_SUCCESS);
    nimcp_thread_cleanup();
}

TEST_F(ConcurrentModuleInitTest, MemorySubsystemConcurrent) {
    nimcp_memory_init();
    const int NUM_THREADS = 4;
    const int ALLOCS = 500;
    std::atomic<int> failures{0};

    std::vector<std::thread> threads;
    for (int i = 0; i < NUM_THREADS; i++) {
        threads.emplace_back([&]() {
            for (int j = 0; j < ALLOCS; j++) {
                void* p = nimcp_malloc(32 + j);
                if (!p) { failures.fetch_add(1); continue; }
                memset(p, 0xFF, 32 + j);
                nimcp_free(p);
            }
        });
    }
    for (auto& t : threads) t.join();

    EXPECT_EQ(failures.load(), 0)
        << "Concurrent nimcp_malloc/free should not fail";
    nimcp_memory_cleanup();
}

TEST_F(ConcurrentModuleInitTest, ResourceLocksUnderContention) {
    nimcp_thread_init();
    const int NUM_THREADS = 8;
    const int OPS = 1000;
    int protected_value = 0;
    std::atomic<int> errors{0};

    std::vector<std::thread> threads;
    for (int i = 0; i < NUM_THREADS; i++) {
        threads.emplace_back([&]() {
            for (int j = 0; j < OPS; j++) {
                nimcp_mutex_t* lock = NULL;
                if (nimcp_get_resource_lock("global_res", &lock) != NIMCP_SUCCESS) {
                    errors.fetch_add(1); continue;
                }
                nimcp_mutex_lock(lock);
                protected_value++;
                nimcp_mutex_unlock(lock);
                nimcp_release_resource_lock("global_res");
            }
        });
    }
    for (auto& t : threads) t.join();

    EXPECT_EQ(errors.load(), 0);
    EXPECT_EQ(protected_value, NUM_THREADS * OPS)
        << "Named resource locks should protect shared state";
    nimcp_thread_cleanup();
}
