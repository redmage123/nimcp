//=============================================================================
// test_tensor_pool.cpp - Unit Tests for Tensor Lifecycle & Stats (Opt 4)
//=============================================================================
// WHAT: Tests tensor create/destroy lifecycle and global stats tracking
// WHY:  Verify tensor allocation, deallocation, and stats consistency
// HOW:  Exercise lifecycle, create/destroy tensors, verify stats

#include <gtest/gtest.h>
#include <cstring>
#include <vector>

extern "C" {
#include "utils/tensor/nimcp_tensor.h"
#include "utils/memory/nimcp_memory.h"
}

//=============================================================================
// Test Fixture
//=============================================================================

class TensorPoolTest : public ::testing::Test {
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
// Lifecycle Tests
//=============================================================================

TEST_F(TensorPoolTest, Init_Shutdown_Idempotent) {
    // Already initialized in SetUp, shutdown shouldn't crash
    nimcp_tensor_shutdown();
    // Re-init for TearDown
    nimcp_tensor_init();
}

TEST_F(TensorPoolTest, CreateDestroy_BasicLifecycle) {
    uint32_t dims[] = {4, 4};
    nimcp_tensor_t* t = nimcp_tensor_create(dims, 2, NIMCP_DTYPE_F32);
    ASSERT_NE(t, nullptr);
    nimcp_tensor_destroy(t);
}

//=============================================================================
// Stats Tracking Tests
//=============================================================================

TEST_F(TensorPoolTest, Stats_InitiallyZero) {
    nimcp_tensor_stats_t stats;
    int rc = nimcp_tensor_get_stats(&stats);
    EXPECT_EQ(rc, NIMCP_TENSOR_OK);
    EXPECT_EQ(stats.tensors_created, 0u);
    EXPECT_EQ(stats.tensors_destroyed, 0u);
}

TEST_F(TensorPoolTest, Stats_CreateIncrements) {
    uint32_t dims[] = {4, 4};
    nimcp_tensor_t* t1 = nimcp_tensor_create(dims, 2, NIMCP_DTYPE_F32);
    nimcp_tensor_t* t2 = nimcp_tensor_create(dims, 2, NIMCP_DTYPE_F32);
    ASSERT_NE(t1, nullptr);
    ASSERT_NE(t2, nullptr);

    nimcp_tensor_stats_t stats;
    nimcp_tensor_get_stats(&stats);
    EXPECT_EQ(stats.tensors_created, 2u);

    nimcp_tensor_destroy(t1);
    nimcp_tensor_destroy(t2);
}

TEST_F(TensorPoolTest, Stats_DestroyIncrements) {
    uint32_t dims[] = {4, 4};
    nimcp_tensor_t* t1 = nimcp_tensor_create(dims, 2, NIMCP_DTYPE_F32);
    nimcp_tensor_t* t2 = nimcp_tensor_create(dims, 2, NIMCP_DTYPE_F32);
    ASSERT_NE(t1, nullptr);
    ASSERT_NE(t2, nullptr);

    nimcp_tensor_destroy(t1);
    nimcp_tensor_destroy(t2);

    nimcp_tensor_stats_t stats;
    nimcp_tensor_get_stats(&stats);
    EXPECT_EQ(stats.tensors_destroyed, 2u);
}

TEST_F(TensorPoolTest, Stats_MemoryTracking) {
    uint32_t dims[] = {8, 8};
    nimcp_tensor_t* t = nimcp_tensor_create(dims, 2, NIMCP_DTYPE_F32);
    ASSERT_NE(t, nullptr);

    nimcp_tensor_stats_t stats;
    nimcp_tensor_get_stats(&stats);
    EXPECT_GT(stats.memory_current, 0u);
    EXPECT_GT(stats.memory_peak, 0u);

    nimcp_tensor_destroy(t);
}

TEST_F(TensorPoolTest, Stats_ResetClearsAll) {
    uint32_t dims[] = {4};
    nimcp_tensor_t* t = nimcp_tensor_create(dims, 1, NIMCP_DTYPE_F32);
    ASSERT_NE(t, nullptr);
    nimcp_tensor_destroy(t);

    nimcp_tensor_reset_stats();

    nimcp_tensor_stats_t stats;
    nimcp_tensor_get_stats(&stats);
    EXPECT_EQ(stats.tensors_created, 0u);
    EXPECT_EQ(stats.tensors_destroyed, 0u);
}

//=============================================================================
// Multiple Create/Destroy Cycles
//=============================================================================

TEST_F(TensorPoolTest, RapidCreateDestroy_32Cycles) {
    uint32_t dims[] = {3, 3};

    for (int i = 0; i < 32; i++) {
        nimcp_tensor_t* t = nimcp_tensor_create(dims, 2, NIMCP_DTYPE_F32);
        ASSERT_NE(t, nullptr) << "Failed on iteration " << i;
        nimcp_tensor_destroy(t);
    }

    nimcp_tensor_stats_t stats;
    nimcp_tensor_get_stats(&stats);
    EXPECT_EQ(stats.tensors_created, 32u);
    EXPECT_EQ(stats.tensors_destroyed, 32u);
}

TEST_F(TensorPoolTest, BatchCreate_ThenBatchDestroy) {
    uint32_t dims[] = {2};
    std::vector<nimcp_tensor_t*> tensors;

    for (int i = 0; i < 16; i++) {
        nimcp_tensor_t* t = nimcp_tensor_create(dims, 1, NIMCP_DTYPE_F32);
        ASSERT_NE(t, nullptr) << "Failed at " << i;
        tensors.push_back(t);
    }

    nimcp_tensor_stats_t stats;
    nimcp_tensor_get_stats(&stats);
    EXPECT_EQ(stats.tensors_created, 16u);
    EXPECT_EQ(stats.tensors_destroyed, 0u);
    EXPECT_GT(stats.memory_current, 0u);

    for (auto* t : tensors) {
        nimcp_tensor_destroy(t);
    }

    nimcp_tensor_get_stats(&stats);
    EXPECT_EQ(stats.tensors_destroyed, 16u);
}

TEST_F(TensorPoolTest, MixedDtypes) {
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

    nimcp_tensor_stats_t stats;
    nimcp_tensor_get_stats(&stats);
    EXPECT_EQ(stats.tensors_created, 3u);
    EXPECT_EQ(stats.tensors_destroyed, 3u);
}
