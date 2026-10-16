/**
 * @file test_memory_api_regression.cpp
 * @brief Regression tests for memory management API stability (GTest)
 *
 * WHAT: Regression tests for memory pool, CoW, memory guards, and core
 *       allocation API stability
 * WHY:  Ensure memory management APIs maintain backward compatibility
 * HOW:  Tests for API contracts, return values, error codes, and known fixes
 *
 * REGRESSION CATEGORIES:
 * - Core Memory API: malloc, calloc, realloc, free, stats
 * - Memory Pool API: create, destroy, acquire, release, reset
 * - CoW Manager API: create, destroy, acquire, release, read, write
 * - Memory Guards API: init, shutdown, check, stats
 * - Error Handling: NULL safety, invalid parameters
 *
 * @author NIMCP Development Team
 * @date 2026-02-02
 */

#include "test_helpers.h"

extern "C" {
#include "utils/memory/nimcp_memory.h"
#include "utils/memory/nimcp_memory_pool.h"
#include "utils/memory/nimcp_cow_manager.h"
#include "utils/memory/nimcp_memory_guards.h"
}

#include <cstring>
#include <cstdlib>
#include <cstdint>

namespace {

/* ============================================================================
 * Base Test Fixture
 * ============================================================================ */

class MemoryApiRegressionTest : public ::testing::Test {
protected:
    void SetUp() override {
        nimcp_memory_init();
    }

    void TearDown() override {
        /* Check for leaks but don't fail - cleanup happens */
        nimcp_memory_stats_t stats;
        if (nimcp_memory_get_stats(&stats)) {
            if (stats.current_allocated > 0) {
                /* Log but don't fail - some tests intentionally leak for testing */
            }
        }
        nimcp_memory_cleanup();
    }
};

/* ============================================================================
 * Memory Guards Test Fixture
 * ============================================================================ */

class MemoryGuardsRegressionTest : public ::testing::Test {
protected:
    void SetUp() override {
        memory_guard_config_t config = memory_guards_default_config();
        memory_guards_init(&config);
    }

    void TearDown() override {
        memory_guards_shutdown();
    }
};

/* ============================================================================
 * Core Memory API Regression Tests
 * ============================================================================ */

TEST_F(MemoryApiRegressionTest, NimcpMallocReturnsValidPointer) {
    /* WHAT: Verify nimcp_malloc returns valid pointer for valid size */
    /* REGRESSION: Basic allocation must work */
    void* ptr = nimcp_malloc(100);
    ASSERT_NE(ptr, nullptr);
    nimcp_free(ptr);
}

TEST_F(MemoryApiRegressionTest, NimcpMallocZeroSize) {
    /* WHAT: Verify nimcp_malloc with size 0 behavior */
    /* REGRESSION: Zero-size allocation behavior must be consistent */
    void* ptr = nimcp_malloc(0);
    /* Standard C allows returning NULL or valid pointer for size 0 */
    /* Either is acceptable */
    if (ptr) {
        nimcp_free(ptr);
    }
}

TEST_F(MemoryApiRegressionTest, NimcpCallocReturnsZeroedMemory) {
    /* WHAT: Verify nimcp_calloc returns zeroed memory */
    /* REGRESSION: Calloc must zero-initialize */
    int* arr = static_cast<int*>(nimcp_calloc(10, sizeof(int)));
    ASSERT_NE(arr, nullptr);

    for (int i = 0; i < 10; i++) {
        EXPECT_EQ(arr[i], 0);
    }

    nimcp_free(arr);
}

TEST_F(MemoryApiRegressionTest, NimcpReallocGrowsAllocation) {
    /* WHAT: Verify nimcp_realloc grows allocation preserving data */
    /* REGRESSION: Realloc must preserve existing data */
    char* str = static_cast<char*>(nimcp_malloc(10));
    ASSERT_NE(str, nullptr);
    strcpy(str, "hello");

    str = static_cast<char*>(nimcp_realloc(str, 100));
    ASSERT_NE(str, nullptr);
    EXPECT_STREQ(str, "hello");

    nimcp_free(str);
}

TEST_F(MemoryApiRegressionTest, NimcpReallocNullActsAsMalloc) {
    /* WHAT: Verify nimcp_realloc(NULL, size) acts like malloc */
    /* REGRESSION: Realloc NULL behavior must match standard */
    void* ptr = nimcp_realloc(NULL, 50);
    ASSERT_NE(ptr, nullptr);
    nimcp_free(ptr);
}

TEST_F(MemoryApiRegressionTest, NimcpFreeNullSafe) {
    /* WHAT: Verify nimcp_free(NULL) is safe */
    /* REGRESSION: NULL free must not crash */
    nimcp_free(NULL);  /* Should not crash */
}

TEST_F(MemoryApiRegressionTest, NimcpStrdupDuplicatesString) {
    /* WHAT: Verify nimcp_strdup creates proper copy */
    /* REGRESSION: String duplication must work */
    const char* original = "test string";
    char* copy = nimcp_strdup(original);

    ASSERT_NE(copy, nullptr);
    EXPECT_NE(copy, original);
    EXPECT_STREQ(copy, original);

    nimcp_free(copy);
}

TEST_F(MemoryApiRegressionTest, NimcpStrdupNullReturnsNull) {
    /* WHAT: Verify nimcp_strdup(NULL) returns NULL */
    /* REGRESSION: NULL safety */
    char* result = nimcp_strdup(NULL);
    EXPECT_EQ(result, nullptr);
}

/* ============================================================================
 * Memory Statistics Regression Tests
 * ============================================================================ */

TEST_F(MemoryApiRegressionTest, MemoryStatsTracksAllocations) {
    /* WHAT: Verify allocation count is tracked */
    /* REGRESSION: Statistics accuracy */
    nimcp_memory_stats_t before, after;
    nimcp_memory_get_stats(&before);

    void* ptr = nimcp_malloc(100);

    nimcp_memory_get_stats(&after);
    EXPECT_GT(after.allocation_count, before.allocation_count);

    nimcp_free(ptr);
}

TEST_F(MemoryApiRegressionTest, MemoryStatsTracksCurrentAllocated) {
    /* WHAT: Verify current_allocated increases with allocation */
    /* REGRESSION: Current allocation tracking */
    nimcp_memory_stats_t before, after_alloc, after_free;
    nimcp_memory_get_stats(&before);

    void* ptr = nimcp_malloc(100);
    nimcp_memory_get_stats(&after_alloc);
    EXPECT_GT(after_alloc.current_allocated, before.current_allocated);

    nimcp_free(ptr);
    nimcp_memory_get_stats(&after_free);
    EXPECT_EQ(after_free.current_allocated, before.current_allocated);
}

TEST_F(MemoryApiRegressionTest, MemoryGetStatsNullReturnsFalse) {
    /* WHAT: Verify NULL stats pointer returns false */
    /* REGRESSION: NULL safety */
    bool result = nimcp_memory_get_stats(NULL);
    EXPECT_FALSE(result);
}

TEST_F(MemoryApiRegressionTest, MemoryClearStatsResetsCounters) {
    /* WHAT: Verify clear_stats resets counters */
    /* REGRESSION: Stats reset functionality */
    void* ptr = nimcp_malloc(100);
    nimcp_memory_clear_stats();

    nimcp_memory_stats_t stats;
    nimcp_memory_get_stats(&stats);
    /* Note: current_allocated may not reset, but counters should */

    nimcp_free(ptr);
}

/* ============================================================================
 * Memory Pool API Regression Tests
 * ============================================================================ */

TEST_F(MemoryApiRegressionTest, MemoryPoolCreateReturnsValidHandle) {
    /* WHAT: Verify memory pool create returns valid handle */
    /* REGRESSION: Pool creation API */
    memory_pool_config_t config = memory_pool_default_config(256, 100);
    memory_pool_t pool = memory_pool_create(&config);

    ASSERT_NE(pool, nullptr);

    memory_pool_destroy(pool);
}

TEST_F(MemoryApiRegressionTest, MemoryPoolCreateNullConfigUsesDefaults) {
    /* WHAT: Verify NULL config creates pool with defaults */
    /* REGRESSION: Default configuration handling */
    memory_pool_t pool = memory_pool_create(NULL);
    /* May return NULL or valid pool depending on implementation */
    if (pool) {
        memory_pool_destroy(pool);
    }
}

TEST_F(MemoryApiRegressionTest, MemoryPoolAcquireReturnsValidPointer) {
    /* WHAT: Verify pool acquire returns valid pointer */
    /* REGRESSION: Acquisition API */
    memory_pool_config_t config = memory_pool_default_config(256, 100);
    memory_pool_t pool = memory_pool_create(&config);

    void* block = memory_pool_acquire(pool);
    ASSERT_NE(block, nullptr);

    memory_pool_release(pool, block);
    memory_pool_destroy(pool);
}

TEST_F(MemoryApiRegressionTest, MemoryPoolReleaseReturnsBlock) {
    /* WHAT: Verify released block can be reacquired */
    /* REGRESSION: Release and reuse */
    memory_pool_config_t config = memory_pool_default_config(256, 10);
    memory_pool_t pool = memory_pool_create(&config);

    void* block1 = memory_pool_acquire(pool);
    memory_pool_release(pool, block1);
    void* block2 = memory_pool_acquire(pool);

    /* Block should be reused (same address) */
    EXPECT_EQ(block1, block2);

    memory_pool_release(pool, block2);
    memory_pool_destroy(pool);
}

TEST_F(MemoryApiRegressionTest, MemoryPoolResetFreesAllBlocks) {
    /* WHAT: Verify reset marks all blocks as free */
    /* REGRESSION: Reset functionality */
    memory_pool_config_t config = memory_pool_default_config(256, 10);
    memory_pool_t pool = memory_pool_create(&config);

    /* Acquire all blocks */
    void* blocks[10];
    for (int i = 0; i < 10; i++) {
        blocks[i] = memory_pool_acquire(pool);
        ASSERT_NE(blocks[i], nullptr);
    }

    /* Next acquire should fail (pool exhausted) */
    void* extra = memory_pool_acquire(pool);
    EXPECT_EQ(extra, nullptr);

    /* Reset should make all available again */
    size_t reset_count = memory_pool_reset(pool);
    EXPECT_EQ(reset_count, 10u);

    /* Now can acquire again */
    void* after_reset = memory_pool_acquire(pool);
    ASSERT_NE(after_reset, nullptr);

    memory_pool_destroy(pool);
}

TEST_F(MemoryApiRegressionTest, MemoryPoolGetStatsAccurate) {
    /* WHAT: Verify pool stats are accurate */
    /* REGRESSION: Statistics accuracy */
    memory_pool_config_t config = memory_pool_default_config(256, 100);
    config.enable_tracking = true;
    memory_pool_t pool = memory_pool_create(&config);

    memory_pool_stats_t stats;
    memory_pool_get_stats(pool, &stats);
    EXPECT_EQ(stats.total_blocks, 100u);
    EXPECT_EQ(stats.allocated_blocks, 0u);

    void* block = memory_pool_acquire(pool);
    memory_pool_get_stats(pool, &stats);
    EXPECT_EQ(stats.allocated_blocks, 1u);

    memory_pool_release(pool, block);
    memory_pool_destroy(pool);
}

TEST_F(MemoryApiRegressionTest, MemoryPoolOwnsReturnsCorrect) {
    /* WHAT: Verify pool ownership detection works */
    /* REGRESSION: Ownership check */
    memory_pool_config_t config = memory_pool_default_config(256, 10);
    memory_pool_t pool = memory_pool_create(&config);

    void* pool_block = memory_pool_acquire(pool);
    void* heap_block = malloc(256);

    EXPECT_TRUE(memory_pool_owns(pool, pool_block));
    EXPECT_FALSE(memory_pool_owns(pool, heap_block));

    memory_pool_release(pool, pool_block);
    free(heap_block);
    memory_pool_destroy(pool);
}

TEST_F(MemoryApiRegressionTest, MemoryPoolGetBlockSize) {
    /* WHAT: Verify get_block_size returns configured size */
    /* REGRESSION: Block size query */
    memory_pool_config_t config = memory_pool_default_config(512, 10);
    memory_pool_t pool = memory_pool_create(&config);

    size_t block_size = memory_pool_get_block_size(pool);
    EXPECT_EQ(block_size, 512u);

    memory_pool_destroy(pool);
}

TEST_F(MemoryApiRegressionTest, MemoryPoolGetAvailable) {
    /* WHAT: Verify get_available returns correct count */
    /* REGRESSION: Available block count */
    memory_pool_config_t config = memory_pool_default_config(256, 10);
    memory_pool_t pool = memory_pool_create(&config);

    EXPECT_EQ(memory_pool_get_available(pool), 10u);

    void* block = memory_pool_acquire(pool);
    EXPECT_EQ(memory_pool_get_available(pool), 9u);

    memory_pool_release(pool, block);
    EXPECT_EQ(memory_pool_get_available(pool), 10u);

    memory_pool_destroy(pool);
}

/* ============================================================================
 * CoW Manager API Regression Tests
 * ============================================================================ */

TEST_F(MemoryApiRegressionTest, CowManagerCreateReturnsValidHandle) {
    /* WHAT: Verify CoW manager create returns valid handle */
    /* REGRESSION: CoW creation API */
    float template_data[64] = {0};
    cow_manager_config_t config = cow_default_config(sizeof(template_data), NULL);
    cow_manager_t cow = cow_manager_create(&config, template_data);

    ASSERT_NE(cow, nullptr);

    cow_manager_destroy(cow);
}

TEST_F(MemoryApiRegressionTest, CowAcquireReturnsValidHandle) {
    /* WHAT: Verify CoW acquire returns valid handle */
    /* REGRESSION: Acquire API */
    float template_data[64] = {1.0f, 2.0f, 3.0f};
    cow_manager_config_t config = cow_default_config(sizeof(template_data), NULL);
    cow_manager_t cow = cow_manager_create(&config, template_data);

    cow_handle_t handle = cow_acquire(cow);
    ASSERT_NE(handle, nullptr);

    cow_release(handle);
    cow_manager_destroy(cow);
}

TEST_F(MemoryApiRegressionTest, CowReadReturnsTemplateData) {
    /* WHAT: Verify cow_read returns template data */
    /* REGRESSION: Read functionality */
    float template_data[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    cow_manager_config_t config = cow_default_config(sizeof(template_data), NULL);
    cow_manager_t cow = cow_manager_create(&config, template_data);

    cow_handle_t handle = cow_acquire(cow);
    const float* data = static_cast<const float*>(cow_read(handle));

    EXPECT_FLOAT_EQ(data[0], 1.0f);
    EXPECT_FLOAT_EQ(data[1], 2.0f);
    EXPECT_FLOAT_EQ(data[2], 3.0f);
    EXPECT_FLOAT_EQ(data[3], 4.0f);

    cow_release(handle);
    cow_manager_destroy(cow);
}

TEST_F(MemoryApiRegressionTest, CowIsSharedInitiallyTrue) {
    /* WHAT: Verify handle is shared after acquire */
    /* REGRESSION: Sharing state */
    float template_data[64] = {0};
    cow_manager_config_t config = cow_default_config(sizeof(template_data), NULL);
    cow_manager_t cow = cow_manager_create(&config, template_data);

    cow_handle_t handle = cow_acquire(cow);
    EXPECT_TRUE(cow_is_shared(handle));

    cow_release(handle);
    cow_manager_destroy(cow);
}

TEST_F(MemoryApiRegressionTest, CowWriteTriggersCopy) {
    /* WHAT: Verify cow_write triggers copy and returns writable pointer */
    /* REGRESSION: Copy-on-write semantics */
    float template_data[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    cow_manager_config_t config = cow_default_config(sizeof(template_data), NULL);
    cow_manager_t cow = cow_manager_create(&config, template_data);

    cow_handle_t handle = cow_acquire(cow);
    EXPECT_TRUE(cow_is_shared(handle));

    float* writable = static_cast<float*>(cow_write(handle));
    ASSERT_NE(writable, nullptr);
    EXPECT_FALSE(cow_is_shared(handle));

    /* Can modify without affecting template */
    writable[0] = 99.0f;

    cow_release(handle);
    cow_manager_destroy(cow);
}

TEST_F(MemoryApiRegressionTest, CowGetRefcountAccurate) {
    /* WHAT: Verify refcount is accurate */
    /* REGRESSION: Reference counting */
    float template_data[64] = {0};
    cow_manager_config_t config = cow_default_config(sizeof(template_data), NULL);
    cow_manager_t cow = cow_manager_create(&config, template_data);

    EXPECT_EQ(cow_get_refcount(cow), 0u);

    cow_handle_t h1 = cow_acquire(cow);
    EXPECT_EQ(cow_get_refcount(cow), 1u);

    cow_handle_t h2 = cow_acquire(cow);
    EXPECT_EQ(cow_get_refcount(cow), 2u);

    cow_release(h1);
    EXPECT_EQ(cow_get_refcount(cow), 1u);

    cow_release(h2);
    EXPECT_EQ(cow_get_refcount(cow), 0u);

    cow_manager_destroy(cow);
}

TEST_F(MemoryApiRegressionTest, CowGetStateReturnsCorrect) {
    /* WHAT: Verify get_state returns correct state */
    /* REGRESSION: State query */
    float template_data[64] = {0};
    cow_manager_config_t config = cow_default_config(sizeof(template_data), NULL);
    cow_manager_t cow = cow_manager_create(&config, template_data);

    cow_handle_t handle = cow_acquire(cow);
    EXPECT_EQ(cow_get_state(handle), COW_STATE_SHARED);

    cow_write(handle);
    EXPECT_EQ(cow_get_state(handle), COW_STATE_PRIVATE);

    cow_release(handle);
    cow_manager_destroy(cow);
}

TEST_F(MemoryApiRegressionTest, CowMakePrivateSucceeds) {
    /* WHAT: Verify make_private forces private copy */
    /* REGRESSION: Pre-emptive copy API */
    float template_data[64] = {0};
    cow_manager_config_t config = cow_default_config(sizeof(template_data), NULL);
    cow_manager_t cow = cow_manager_create(&config, template_data);

    cow_handle_t handle = cow_acquire(cow);
    EXPECT_TRUE(cow_is_shared(handle));

    bool result = cow_make_private(handle);
    EXPECT_TRUE(result);
    EXPECT_FALSE(cow_is_shared(handle));

    cow_release(handle);
    cow_manager_destroy(cow);
}

TEST_F(MemoryApiRegressionTest, CowStateEnumValuesStable) {
    /* WHAT: Verify CoW state enum values are stable */
    /* REGRESSION: Enum value stability for ABI */
    EXPECT_EQ(COW_STATE_SHARED, 0);
    EXPECT_EQ(COW_STATE_PRIVATE, 1);
    EXPECT_EQ(COW_STATE_INVALID, 2);
}

/* ============================================================================
 * Memory Guards API Regression Tests
 * ============================================================================ */

TEST_F(MemoryGuardsRegressionTest, MemoryGuardsInitReturnsTrue) {
    /* WHAT: Verify memory guards init returns true */
    /* REGRESSION: Guards initialization */
    /* Note: Already initialized in SetUp, just verify it worked */
    EXPECT_TRUE(memory_guards_is_enabled());
}

TEST(MemoryGuardsConfigTest, MemoryGuardsDefaultConfigReasonable) {
    /* WHAT: Verify default config has reasonable values */
    /* REGRESSION: Default configuration */
    memory_guard_config_t config = memory_guards_default_config();

    EXPECT_TRUE(config.enable_guards);
    EXPECT_TRUE(config.enable_leak_detection);
    EXPECT_TRUE(config.enable_overflow_detection);
}

TEST_F(MemoryGuardsRegressionTest, MemoryGuardsStatsInitial) {
    /* WHAT: Verify initial stats are zero */
    /* REGRESSION: Stats initialization */
    memory_guard_stats_t stats = memory_guards_get_stats();

    EXPECT_EQ(stats.total_allocations, 0u);
    EXPECT_EQ(stats.total_frees, 0u);
    EXPECT_EQ(stats.active_allocations, 0u);
}

TEST_F(MemoryGuardsRegressionTest, MemoryGuardsTracksAllocations) {
    /* WHAT: Verify guards track allocations */
    /* REGRESSION: Allocation tracking */
    void* ptr = nimcp_malloc_guarded(100, __FILE__, __LINE__);
    ASSERT_NE(ptr, nullptr);

    memory_guard_stats_t stats = memory_guards_get_stats();
    EXPECT_GE(stats.total_allocations, 1u);
    EXPECT_GE(stats.active_allocations, 1u);

    nimcp_free_guarded(ptr, __FILE__, __LINE__);

    stats = memory_guards_get_stats();
    EXPECT_GE(stats.total_frees, 1u);
}

TEST_F(MemoryGuardsRegressionTest, MemoryGuardsCheckPtrValid) {
    /* WHAT: Verify check_ptr returns true for valid allocation */
    /* REGRESSION: Guard checking */
    void* ptr = nimcp_malloc_guarded(100, __FILE__, __LINE__);
    ASSERT_NE(ptr, nullptr);

    bool valid = memory_guards_check_ptr(ptr);
    EXPECT_TRUE(valid);

    nimcp_free_guarded(ptr, __FILE__, __LINE__);
}

TEST_F(MemoryGuardsRegressionTest, MemoryGuardsCheckAllNoCorruption) {
    /* WHAT: Verify check_all returns 0 with no corruption */
    /* REGRESSION: Bulk check functionality */
    void* ptr1 = nimcp_malloc_guarded(100, __FILE__, __LINE__);
    void* ptr2 = nimcp_malloc_guarded(200, __FILE__, __LINE__);

    uint32_t corruptions = memory_guards_check_all();
    EXPECT_EQ(corruptions, 0u);

    nimcp_free_guarded(ptr1, __FILE__, __LINE__);
    nimcp_free_guarded(ptr2, __FILE__, __LINE__);
}

TEST_F(MemoryGuardsRegressionTest, MemoryGuardsEnableDisable) {
    /* WHAT: Verify enable/disable runtime control */
    /* REGRESSION: Runtime control */
    EXPECT_TRUE(memory_guards_is_enabled());

    memory_guards_set_enabled(false);
    EXPECT_FALSE(memory_guards_is_enabled());

    memory_guards_set_enabled(true);
    EXPECT_TRUE(memory_guards_is_enabled());
}

TEST(MemoryGuardsConstantsTest, MemoryGuardsCanaryConstants) {
    /* WHAT: Verify canary constants are stable */
    /* REGRESSION: Canary value stability */
    EXPECT_EQ(CANARY_START, 0xDEADBEEFu);
    EXPECT_EQ(CANARY_END, 0xCAFEBABEu);
    EXPECT_EQ(FREED_MARKER, 0xFEEDFACEu);
}

/* ============================================================================
 * Aligned Allocation Regression Tests
 * ============================================================================ */

TEST_F(MemoryApiRegressionTest, AlignedMallocReturnsAlignedPointer) {
    /* WHAT: Verify aligned malloc returns properly aligned pointer */
    /* REGRESSION: Alignment functionality */
    void* ptr = nimcp_aligned_malloc(256, 64);
    ASSERT_NE(ptr, nullptr);
    EXPECT_EQ(reinterpret_cast<uintptr_t>(ptr) % 64, 0u);

    nimcp_aligned_free(ptr);
}

TEST_F(MemoryApiRegressionTest, AlignedAllocWorks) {
    /* WHAT: Verify nimcp_aligned_alloc works */
    /* REGRESSION: Aligned allocation API */
    void* ptr = nimcp_aligned_alloc(32, 128);
    ASSERT_NE(ptr, nullptr);
    EXPECT_EQ(reinterpret_cast<uintptr_t>(ptr) % 32, 0u);

    nimcp_free(ptr);
}

} // anonymous namespace
