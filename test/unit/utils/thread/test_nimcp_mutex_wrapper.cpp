/** @file test_nimcp_mutex_wrapper.cpp - P1 audit: mutex wrapper tests */
#include <gtest/gtest.h>
#include <atomic>
#include <thread>
#include <vector>
#include <string>
#include <chrono>

extern "C" {
#include "utils/thread/nimcp_thread.h"
}

class MutexWrapperTest : public ::testing::Test {
protected:
    void SetUp() override { nimcp_thread_init(); }
    void TearDown() override { nimcp_thread_cleanup(); }
};

TEST_F(MutexWrapperTest, InitLockUnlockDestroy) {
    nimcp_mutex_t mutex;
    ASSERT_EQ(nimcp_mutex_init(&mutex, NULL), NIMCP_SUCCESS);
    EXPECT_EQ(nimcp_mutex_lock(&mutex), NIMCP_SUCCESS);
    EXPECT_EQ(nimcp_mutex_unlock(&mutex), NIMCP_SUCCESS);
    EXPECT_EQ(nimcp_mutex_destroy(&mutex), NIMCP_SUCCESS);
}

TEST_F(MutexWrapperTest, InitWithNormalType) {
    nimcp_mutex_t mutex;
    mutex_attr_t attr; attr.type = MUTEX_TYPE_NORMAL;
    ASSERT_EQ(nimcp_mutex_init(&mutex, &attr), NIMCP_SUCCESS);
    EXPECT_EQ(nimcp_mutex_lock(&mutex), NIMCP_SUCCESS);
    EXPECT_EQ(nimcp_mutex_unlock(&mutex), NIMCP_SUCCESS);
    nimcp_mutex_destroy(&mutex);
}

TEST_F(MutexWrapperTest, CreateAndFree) {
    nimcp_mutex_t* mutex = nimcp_mutex_create(NULL);
    ASSERT_NE(mutex, nullptr);
    EXPECT_EQ(nimcp_mutex_lock(mutex), NIMCP_SUCCESS);
    EXPECT_EQ(nimcp_mutex_unlock(mutex), NIMCP_SUCCESS);
    EXPECT_EQ(nimcp_mutex_free(mutex), NIMCP_SUCCESS);
}

TEST_F(MutexWrapperTest, StaticInitializer) {
    nimcp_mutex_t mutex = NIMCP_MUTEX_INITIALIZER;
    EXPECT_EQ(nimcp_mutex_lock(&mutex), NIMCP_SUCCESS);
    EXPECT_EQ(nimcp_mutex_unlock(&mutex), NIMCP_SUCCESS);
    nimcp_mutex_destroy(&mutex);
}

TEST_F(MutexWrapperTest, StaticInitializerConcurrent) {
    nimcp_mutex_t mutex = NIMCP_MUTEX_INITIALIZER;
    int counter = 0;
    std::vector<std::thread> threads;
    for (int i = 0; i < 8; i++) {
        threads.emplace_back([&]() {
            for (int j = 0; j < 10000; j++) {
                nimcp_mutex_lock(&mutex);
                counter++;
                nimcp_mutex_unlock(&mutex);
            }
        });
    }
    for (auto& t : threads) t.join();
    EXPECT_EQ(counter, 80000);
    nimcp_mutex_destroy(&mutex);
}

TEST_F(MutexWrapperTest, RecursiveMutexDoubleLock) {
    nimcp_mutex_t mutex;
    mutex_attr_t attr; attr.type = MUTEX_TYPE_RECURSIVE;
    ASSERT_EQ(nimcp_mutex_init(&mutex, &attr), NIMCP_SUCCESS);
    EXPECT_EQ(nimcp_mutex_lock(&mutex), NIMCP_SUCCESS);
    EXPECT_EQ(nimcp_mutex_lock(&mutex), NIMCP_SUCCESS);
    EXPECT_EQ(nimcp_mutex_unlock(&mutex), NIMCP_SUCCESS);
    EXPECT_EQ(nimcp_mutex_unlock(&mutex), NIMCP_SUCCESS);
    nimcp_mutex_destroy(&mutex);
}

TEST_F(MutexWrapperTest, RecursiveMutexTripleLock) {
    mutex_attr_t attr; attr.type = MUTEX_TYPE_RECURSIVE;
    nimcp_mutex_t* mutex = nimcp_mutex_create(&attr);
    ASSERT_NE(mutex, nullptr);
    for (int i = 0; i < 3; i++) EXPECT_EQ(nimcp_mutex_lock(mutex), NIMCP_SUCCESS);
    for (int i = 0; i < 3; i++) EXPECT_EQ(nimcp_mutex_unlock(mutex), NIMCP_SUCCESS);
    nimcp_mutex_free(mutex);
}

TEST_F(MutexWrapperTest, TrylockSucceeds) {
    nimcp_mutex_t mutex;
    nimcp_mutex_init(&mutex, NULL);
    EXPECT_EQ(nimcp_mutex_trylock(&mutex), NIMCP_SUCCESS);
    nimcp_mutex_unlock(&mutex);
    nimcp_mutex_destroy(&mutex);
}

TEST_F(MutexWrapperTest, TrylockBusy) {
    nimcp_mutex_t mutex;
    nimcp_mutex_init(&mutex, NULL);
    nimcp_mutex_lock(&mutex);
    std::atomic<nimcp_result_t> res{NIMCP_SUCCESS};
    std::thread t([&]() { res.store(nimcp_mutex_trylock(&mutex)); });
    t.join();
    EXPECT_EQ(res.load(), NIMCP_BUSY);
    nimcp_mutex_unlock(&mutex);
    nimcp_mutex_destroy(&mutex);
}

TEST_F(MutexWrapperTest, ErrorcheckMutex) {
    nimcp_mutex_t mutex;
    mutex_attr_t attr; attr.type = MUTEX_TYPE_ERRORCHECK;
    ASSERT_EQ(nimcp_mutex_init(&mutex, &attr), NIMCP_SUCCESS);
    EXPECT_EQ(nimcp_mutex_lock(&mutex), NIMCP_SUCCESS);
    EXPECT_EQ(nimcp_mutex_unlock(&mutex), NIMCP_SUCCESS);
    nimcp_mutex_destroy(&mutex);
}

static std::atomic<int> g_once_ctr{0};
static void once_fn(void) { g_once_ctr.fetch_add(1); }

TEST_F(MutexWrapperTest, OnceExactlyOnce) {
    g_once_ctr.store(0);
    nimcp_once_t ctl = NIMCP_ONCE_INIT;
    for (int i = 0; i < 5; i++) EXPECT_EQ(nimcp_once(&ctl, once_fn), NIMCP_SUCCESS);
    EXPECT_EQ(g_once_ctr.load(), 1);
}

TEST_F(MutexWrapperTest, OnceConcurrent) {
    g_once_ctr.store(0);
    nimcp_once_t ctl = NIMCP_ONCE_INIT;
    std::vector<std::thread> threads;
    for (int i = 0; i < 16; i++)
        threads.emplace_back([&]() { nimcp_once(&ctl, once_fn); });
    for (auto& t : threads) t.join();
    EXPECT_EQ(g_once_ctr.load(), 1);
}

TEST_F(MutexWrapperTest, ThreadLocalNameIsolation) {
    const int N = 4;
    std::vector<std::string> names(N);
    std::atomic<int> bar{0};
    std::vector<std::thread> threads;
    for (int i = 0; i < N; i++) {
        threads.emplace_back([&, i]() {
            char nm[NIMCP_THREAD_NAME_MAX];
            snprintf(nm, sizeof(nm), "thr_%d", i);
            nimcp_thread_set_name(nm);
            bar.fetch_add(1);
            while (bar.load() < N) {}
            char buf[NIMCP_THREAD_NAME_MAX];
            nimcp_thread_get_name(buf, sizeof(buf));
            names[i] = buf;
        });
    }
    for (auto& t : threads) t.join();
    for (int i = 0; i < N; i++) {
        char exp[NIMCP_THREAD_NAME_MAX];
        snprintf(exp, sizeof(exp), "thr_%d", i);
        EXPECT_EQ(names[i], exp);
    }
}

TEST_F(MutexWrapperTest, SpinlockBasic) {
    nimcp_spinlock_t lock;
    ASSERT_EQ(nimcp_spinlock_init(&lock), NIMCP_SUCCESS);
    EXPECT_EQ(nimcp_spinlock_lock(&lock), NIMCP_SUCCESS);
    EXPECT_EQ(nimcp_spinlock_unlock(&lock), NIMCP_SUCCESS);
    nimcp_spinlock_destroy(&lock);
}

TEST_F(MutexWrapperTest, RWLockBasic) {
    nimcp_rwlock_t rw;
    ASSERT_EQ(nimcp_rwlock_init(&rw), NIMCP_SUCCESS);
    EXPECT_EQ(nimcp_rwlock_rdlock(&rw), NIMCP_SUCCESS);
    EXPECT_EQ(nimcp_rwlock_unlock(&rw), NIMCP_SUCCESS);
    EXPECT_EQ(nimcp_rwlock_wrlock(&rw), NIMCP_SUCCESS);
    EXPECT_EQ(nimcp_rwlock_unlock(&rw), NIMCP_SUCCESS);
    nimcp_rwlock_destroy(&rw);
}

TEST_F(MutexWrapperTest, RWLockConcurrentReaders) {
    nimcp_rwlock_t rw;
    nimcp_rwlock_init(&rw);
    std::atomic<int> active{0}, peak{0};
    std::vector<std::thread> threads;
    for (int i = 0; i < 8; i++) {
        threads.emplace_back([&]() {
            nimcp_rwlock_rdlock(&rw);
            int c = active.fetch_add(1) + 1;
            int p = peak.load();
            while (c > p && !peak.compare_exchange_weak(p, c)) {}
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            active.fetch_sub(1);
            nimcp_rwlock_unlock(&rw);
        });
    }
    for (auto& t : threads) t.join();
    EXPECT_GT(peak.load(), 1);
    nimcp_rwlock_destroy(&rw);
}

TEST_F(MutexWrapperTest, CondVarSignal) {
    nimcp_mutex_t mtx;
    nimcp_cond_t cv;
    nimcp_mutex_init(&mtx, NULL);
    nimcp_cond_init(&cv);
    bool ready = false;
    std::thread c([&]() {
        nimcp_mutex_lock(&mtx);
        while (!ready) nimcp_cond_wait(&cv, &mtx);
        nimcp_mutex_unlock(&mtx);
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    nimcp_mutex_lock(&mtx);
    ready = true;
    nimcp_cond_signal(&cv);
    nimcp_mutex_unlock(&mtx);
    c.join();
    EXPECT_TRUE(ready);
    nimcp_cond_destroy(&cv);
    nimcp_mutex_destroy(&mtx);
}

TEST_F(MutexWrapperTest, ResourceLock) {
    nimcp_mutex_t* lk = NULL;
    ASSERT_EQ(nimcp_get_resource_lock("test_res", &lk), NIMCP_SUCCESS);
    ASSERT_NE(lk, nullptr);
    EXPECT_EQ(nimcp_mutex_lock(lk), NIMCP_SUCCESS);
    EXPECT_EQ(nimcp_mutex_unlock(lk), NIMCP_SUCCESS);
    EXPECT_EQ(nimcp_release_resource_lock("test_res"), NIMCP_SUCCESS);
}

TEST_F(MutexWrapperTest, ResourceLockSameId) {
    nimcp_mutex_t *l1 = NULL, *l2 = NULL;
    nimcp_get_resource_lock("shared", &l1);
    nimcp_get_resource_lock("shared", &l2);
    EXPECT_EQ(l1, l2);
    nimcp_release_resource_lock("shared");
    nimcp_release_resource_lock("shared");
}
