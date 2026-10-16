//=============================================================================
// test_tensor_pool_regression.cpp - Regression Tests for Tensor Lifecycle
//=============================================================================
// WHAT: Regression tests for tensor create/destroy stability and stats
// WHY:  Ensure no regressions: no leaks, no crashes on rapid create/destroy
// HOW:  Stress-test with high volumes, verify stats consistency

#include <gtest/gtest.h>
#include <vector>
#include <thread>
#include <atomic>

extern "C" {
#include "utils/tensor/nimcp_tensor.h"
#include "utils/memory/nimcp_memory.h"
}

//=============================================================================
// Test Fixture
//=============================================================================

class TensorPoolRegressionTest : public ::testing::Test {
protected:
    void SetUp() override {
        nimcp_memory_init();
        nimcp_tensor_init();
        nimcp_tensor_reset_stats();
    }

    void TearDown() override {
        nimcp_tensor_shutdown();
    }
};

//=============================================================================
// Stability: Rapid Create/Destroy Cycles
//=============================================================================

TEST_F(TensorPoolRegressionTest, RapidCreateDestroy_1000Cycles) {
    uint32_t dims[] = {8, 8};

    for (int i = 0; i < 1000; i++) {
        nimcp_tensor_t* t = nimcp_tensor_create(dims, 2, NIMCP_DTYPE_F32);
        ASSERT_NE(t, nullptr) << "Failed at cycle " << i;
        nimcp_tensor_destroy(t);
    }

    nimcp_tensor_stats_t stats;
    nimcp_tensor_get_stats(&stats);
    EXPECT_EQ(stats.tensors_created, 1000u);
    EXPECT_EQ(stats.tensors_destroyed, 1000u);
}

//=============================================================================
// Stability: Batch Create Then Batch Destroy
//=============================================================================

TEST_F(TensorPoolRegressionTest, BatchCreateBatchDestroy_256Tensors) {
    uint32_t dims[] = {4};
    std::vector<nimcp_tensor_t*> tensors;

    for (int i = 0; i < 256; i++) {
        nimcp_tensor_t* t = nimcp_tensor_create(dims, 1, NIMCP_DTYPE_F32);
        ASSERT_NE(t, nullptr) << "Failed at " << i;
        tensors.push_back(t);
    }

    for (auto* t : tensors) {
        nimcp_tensor_destroy(t);
    }

    nimcp_tensor_stats_t stats;
    nimcp_tensor_get_stats(&stats);
    EXPECT_EQ(stats.tensors_created, 256u);
    EXPECT_EQ(stats.tensors_destroyed, 256u);
}

//=============================================================================
// Stability: Mixed Tensor Types
//=============================================================================

TEST_F(TensorPoolRegressionTest, MixedDtypes_AllWork) {
    uint32_t dims[] = {4, 4};

    nimcp_tensor_t* t_f32 = nimcp_tensor_create(dims, 2, NIMCP_DTYPE_F32);
    nimcp_tensor_t* t_f64 = nimcp_tensor_create(dims, 2, NIMCP_DTYPE_F64);
    nimcp_tensor_t* t_i32 = nimcp_tensor_create(dims, 2, NIMCP_DTYPE_I32);

    EXPECT_NE(t_f32, nullptr);
    EXPECT_NE(t_f64, nullptr);
    EXPECT_NE(t_i32, nullptr);

    nimcp_tensor_destroy(t_f32);
    nimcp_tensor_destroy(t_f64);
    nimcp_tensor_destroy(t_i32);
}

//=============================================================================
// Stability: Repeated Init/Shutdown Cycles
//=============================================================================

TEST_F(TensorPoolRegressionTest, RepeatedInitShutdown_NoLeak) {
    for (int i = 0; i < 10; i++) {
        nimcp_tensor_shutdown();
        nimcp_tensor_init();

        uint32_t dims[] = {2};
        nimcp_tensor_t* t = nimcp_tensor_create(dims, 1, NIMCP_DTYPE_F32);
        ASSERT_NE(t, nullptr);
        nimcp_tensor_destroy(t);
    }
}

//=============================================================================
// Stats Consistency
//=============================================================================

TEST_F(TensorPoolRegressionTest, Stats_CreatedEqualsDestroyedAfterCleanup) {
    uint32_t dims[] = {2, 2};

    std::vector<nimcp_tensor_t*> tensors;
    for (int i = 0; i < 50; i++) {
        nimcp_tensor_t* t = nimcp_tensor_create(dims, 2, NIMCP_DTYPE_F32);
        ASSERT_NE(t, nullptr);
        tensors.push_back(t);
    }

    nimcp_tensor_stats_t stats;
    nimcp_tensor_get_stats(&stats);
    EXPECT_EQ(stats.tensors_created, 50u);
    EXPECT_EQ(stats.tensors_destroyed, 0u);

    for (auto* t : tensors) {
        nimcp_tensor_destroy(t);
    }

    nimcp_tensor_get_stats(&stats);
    EXPECT_EQ(stats.tensors_created, 50u);
    EXPECT_EQ(stats.tensors_destroyed, 50u);
}

//=============================================================================
// Thread Safety
//=============================================================================

TEST_F(TensorPoolRegressionTest, ConcurrentCreateDestroy_NoCorruption) {
    std::atomic<int> failures{0};
    constexpr int NUM_THREADS = 4;
    constexpr int OPS_PER_THREAD = 50;

    auto worker = [&failures]() {
        uint32_t dims[] = {4};
        for (int i = 0; i < OPS_PER_THREAD; i++) {
            nimcp_tensor_t* t = nimcp_tensor_create(dims, 1, NIMCP_DTYPE_F32);
            if (!t) {
                failures.fetch_add(1);
                continue;
            }
            nimcp_tensor_destroy(t);
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < NUM_THREADS; i++) {
        threads.emplace_back(worker);
    }
    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(failures.load(), 0);
}
