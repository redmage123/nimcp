/** @file test_nimcp_memory_wrapper.cpp - P2 audit: memory wrapper tests */
#include <gtest/gtest.h>
#include <cstring>
#include <vector>

extern "C" {
#include "utils/memory/nimcp_memory.h"
}

class MemoryWrapperTest : public ::testing::Test {
protected:
    void SetUp() override {
        nimcp_memory_init();
        nimcp_memory_enable_tracking(true);
        nimcp_memory_reset_state();
    }
    void TearDown() override {
        nimcp_memory_cleanup();
    }
};

TEST_F(MemoryWrapperTest, BasicAllocFree) {
    void* ptr = nimcp_malloc(100);
    ASSERT_NE(ptr, nullptr);
    memset(ptr, 0xAB, 100);
    nimcp_free(ptr);
}

TEST_F(MemoryWrapperTest, CallocZeroesMemory) {
    unsigned char* ptr = (unsigned char*)nimcp_calloc(10, sizeof(int));
    ASSERT_NE(ptr, nullptr);
    for (size_t i = 0; i < 10 * sizeof(int); i++) {
        EXPECT_EQ(ptr[i], 0) << "calloc should zero memory at byte " << i;
    }
    nimcp_free(ptr);
}

TEST_F(MemoryWrapperTest, ReallocGrows) {
    int* ptr = (int*)nimcp_malloc(4 * sizeof(int));
    ASSERT_NE(ptr, nullptr);
    for (int i = 0; i < 4; i++) ptr[i] = i + 1;
    ptr = (int*)nimcp_realloc(ptr, 8 * sizeof(int));
    ASSERT_NE(ptr, nullptr);
    for (int i = 0; i < 4; i++) EXPECT_EQ(ptr[i], i + 1);
    for (int i = 4; i < 8; i++) ptr[i] = i + 1;
    nimcp_free(ptr);
}

TEST_F(MemoryWrapperTest, ReallocShrinks) {
    int* ptr = (int*)nimcp_malloc(8 * sizeof(int));
    ASSERT_NE(ptr, nullptr);
    for (int i = 0; i < 8; i++) ptr[i] = i + 100;
    ptr = (int*)nimcp_realloc(ptr, 2 * sizeof(int));
    ASSERT_NE(ptr, nullptr);
    EXPECT_EQ(ptr[0], 100);
    EXPECT_EQ(ptr[1], 101);
    nimcp_free(ptr);
}

TEST_F(MemoryWrapperTest, NullFreeIsSafe) {
    nimcp_free(NULL);
    SUCCEED() << "nimcp_free(NULL) should not crash";
}

TEST_F(MemoryWrapperTest, ZeroSizeAllocation) {
    void* ptr = nimcp_malloc(0);
    // C standard: malloc(0) may return NULL or a unique pointer
    // Either is acceptable; just ensure no crash on free
    nimcp_free(ptr);
    SUCCEED();
}

TEST_F(MemoryWrapperTest, ReallocNullActsAsMalloc) {
    void* ptr = nimcp_realloc(NULL, 64);
    ASSERT_NE(ptr, nullptr) << "realloc(NULL, size) should act as malloc";
    memset(ptr, 0xFF, 64);
    nimcp_free(ptr);
}

TEST_F(MemoryWrapperTest, StatsTracking) {
    nimcp_memory_stats_t stats;
    void* p1 = nimcp_malloc(100);
    void* p2 = nimcp_malloc(200);
    ASSERT_TRUE(nimcp_memory_get_stats(&stats));
    EXPECT_GE(stats.allocation_count, 2u);
    EXPECT_GE(stats.current_allocated, 300u);
    nimcp_free(p1);
    nimcp_free(p2);
    ASSERT_TRUE(nimcp_memory_get_stats(&stats));
    EXPECT_GE(stats.free_count, 2u);
}

TEST_F(MemoryWrapperTest, StrdupWorks) {
    const char* orig = "hello nimcp";
    char* copy = nimcp_strdup(orig);
    ASSERT_NE(copy, nullptr);
    EXPECT_STREQ(copy, orig);
    EXPECT_NE((void*)copy, (void*)orig);
    nimcp_free(copy);
}

TEST_F(MemoryWrapperTest, MultipleAllocFree) {
    std::vector<void*> ptrs;
    for (int i = 0; i < 100; i++) {
        void* p = nimcp_malloc(64 + i);
        ASSERT_NE(p, nullptr) << "Allocation " << i << " failed";
        memset(p, (unsigned char)i, 64 + i);
        ptrs.push_back(p);
    }
    for (void* p : ptrs) nimcp_free(p);
}
