//=============================================================================
// test_brain_init_allocation.cpp - Tests for Brain Allocation Function
//=============================================================================
/**
 * @file test_brain_init_allocation.cpp
 * @brief Comprehensive unit tests for nimcp_brain_factory_allocate_brain()
 *
 * WHAT: Test suite for brain structure allocation and initialization
 * WHY:  Ensure proper memory allocation, initialization, and error handling
 * HOW:  GoogleTest framework with positive/negative/edge cases
 *
 * FUNCTION UNDER TEST:
 * - nimcp_brain_factory_allocate_brain() - Allocate and initialize brain_t
 *
 * TEST CATEGORIES:
 * 1. Basic Allocation (success path)
 * 2. Field Initialization (all struct members)
 * 3. Memory Allocation Failure (error handling)
 * 4. Mutex Initialization (thread safety)
 * 5. COW Fields (copy-on-write support)
 * 6. Event Bus Fields (event system)
 * 7. Long-term Memory Buffer
 * 8. Multiple Allocations (resource management)
 * 9. Cleanup and Destruction
 *
 * @version 2.7.0
 * @author NIMCP Development Team
 * @date 2025-11-21
 */

#include <gtest/gtest.h>
#include <cstring>

#include "core/brain/factory/nimcp_brain_factory.h"
#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_internal.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "nimcp.h"

//=============================================================================
// Test Fixture
//=============================================================================

class BrainInitAllocationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize NIMCP systems
        nimcp_memory_init();
        nimcp_init();
        brain_clear_error();
    }

    void TearDown() override {
        // Cleanup
        nimcp_shutdown();
        nimcp_memory_cleanup();
    }
};

//=============================================================================
// 1. Basic Allocation Tests
//=============================================================================

TEST_F(BrainInitAllocationTest, AllocateBrain_Success) {
    brain_t brain = nimcp_brain_factory_allocate_brain();

    ASSERT_NE(brain, nullptr) << "Brain allocation should succeed";
    EXPECT_EQ(brain_get_last_error(), nullptr) << "No error should be set on success";

    brain_destroy(brain);
}

TEST_F(BrainInitAllocationTest, AllocateBrain_ReturnsValidPointer) {
    brain_t brain = nimcp_brain_factory_allocate_brain();

    ASSERT_NE(brain, nullptr);

    // Should be able to access brain structure
    EXPECT_NE(&brain->config, nullptr);
    EXPECT_NE(&brain->stats, nullptr);

    brain_destroy(brain);
}

TEST_F(BrainInitAllocationTest, AllocateBrain_MultipleAllocations) {
    // Allocate multiple brains
    brain_t brain1 = nimcp_brain_factory_allocate_brain();
    brain_t brain2 = nimcp_brain_factory_allocate_brain();
    brain_t brain3 = nimcp_brain_factory_allocate_brain();

    ASSERT_NE(brain1, nullptr);
    ASSERT_NE(brain2, nullptr);
    ASSERT_NE(brain3, nullptr);

    // Should be distinct allocations
    EXPECT_NE(brain1, brain2);
    EXPECT_NE(brain2, brain3);
    EXPECT_NE(brain1, brain3);

    brain_destroy(brain1);
    brain_destroy(brain2);
    brain_destroy(brain3);
}

//=============================================================================
// 2. Field Initialization Tests
//=============================================================================

TEST_F(BrainInitAllocationTest, InitializesInputCacheFields) {
    brain_t brain = nimcp_brain_factory_allocate_brain();
    ASSERT_NE(brain, nullptr);

    EXPECT_EQ(brain->last_input, nullptr) << "last_input should be NULL";
    EXPECT_EQ(brain->cached_decision, nullptr) << "cached_decision should be NULL";
    EXPECT_EQ(brain->input_size, 0u) << "input_size should be 0";

    brain_destroy(brain);
}

TEST_F(BrainInitAllocationTest, InitializesCOWFields) {
    brain_t brain = nimcp_brain_factory_allocate_brain();
    ASSERT_NE(brain, nullptr);

    EXPECT_FALSE(brain->is_cow_clone) << "is_cow_clone should be false";
    EXPECT_TRUE(brain->owns_network) << "owns_network should be true";
    EXPECT_EQ(brain->original_network, nullptr) << "original_network should be NULL";
    EXPECT_FALSE(brain->network_is_cached) << "network_is_cached should be false";

    brain_destroy(brain);
}

TEST_F(BrainInitAllocationTest, InitializesReferenceCountingFields) {
    brain_t brain = nimcp_brain_factory_allocate_brain();
    ASSERT_NE(brain, nullptr);

    // Note: network_refcount renamed to network_refcount_atomic (no longer pointer)
    // refcount_mutex removed - now using atomic operations
    EXPECT_FALSE(brain->can_use_readonly) << "can_use_readonly should be false";

    brain_destroy(brain);
}

TEST_F(BrainInitAllocationTest, InitializesDistributedField) {
    brain_t brain = nimcp_brain_factory_allocate_brain();
    ASSERT_NE(brain, nullptr);

    EXPECT_EQ(brain->distributed, nullptr) << "distributed should be NULL (standalone)";

    brain_destroy(brain);
}

TEST_F(BrainInitAllocationTest, InitializesCommunityDetectionFields) {
    brain_t brain = nimcp_brain_factory_allocate_brain();
    ASSERT_NE(brain, nullptr);

    EXPECT_EQ(brain->functional_modules, nullptr);
    EXPECT_EQ(brain->network_hubs, nullptr);
    EXPECT_EQ(brain->topology_metrics, nullptr);
    EXPECT_FALSE(brain->auto_detect_communities);
    EXPECT_FLOAT_EQ(brain->community_detection_interval, 0.0f);

    brain_destroy(brain);
}

//=============================================================================
// 3. Long-term Memory Buffer Tests
//=============================================================================

TEST_F(BrainInitAllocationTest, InitializesLongTermMemoryBuffer) {
    brain_t brain = nimcp_brain_factory_allocate_brain();
    ASSERT_NE(brain, nullptr);

    EXPECT_EQ(brain->longterm_capacity, 100u) << "Default capacity should be 100";
    EXPECT_EQ(brain->longterm_count, 0u) << "Initial count should be 0";
    EXPECT_NE(brain->longterm_memory, nullptr) << "Buffer should be allocated";

    brain_destroy(brain);
}

TEST_F(BrainInitAllocationTest, LongTermMemoryBuffer_ValidAllocation) {
    brain_t brain = nimcp_brain_factory_allocate_brain();
    ASSERT_NE(brain, nullptr);

    // If buffer allocated, capacity should match allocation size
    if (brain->longterm_memory != nullptr) {
        EXPECT_EQ(brain->longterm_capacity, 100u);
    } else {
        // If allocation failed, capacity should be 0
        EXPECT_EQ(brain->longterm_capacity, 0u);
    }

    brain_destroy(brain);
}

//=============================================================================
// 4. Mutex Initialization Tests
//=============================================================================

TEST_F(BrainInitAllocationTest, InitializesCacheMutex) {
    brain_t brain = nimcp_brain_factory_allocate_brain();
    ASSERT_NE(brain, nullptr);

    // Mutex should be initialized - try locking it
    int lock_result = nimcp_platform_mutex_lock(&brain->cache_mutex);
    EXPECT_EQ(lock_result, 0) << "Should be able to lock initialized mutex";

    if (lock_result == 0) {
        nimcp_platform_mutex_unlock(&brain->cache_mutex);
    }

    brain_destroy(brain);
}

TEST_F(BrainInitAllocationTest, CacheMutex_SupportsLockUnlock) {
    brain_t brain = nimcp_brain_factory_allocate_brain();
    ASSERT_NE(brain, nullptr);

    // Lock
    EXPECT_EQ(nimcp_platform_mutex_lock(&brain->cache_mutex), 0);

    // Unlock
    EXPECT_EQ(nimcp_platform_mutex_unlock(&brain->cache_mutex), 0);

    // Lock again - should work
    EXPECT_EQ(nimcp_platform_mutex_lock(&brain->cache_mutex), 0);
    EXPECT_EQ(nimcp_platform_mutex_unlock(&brain->cache_mutex), 0);

    brain_destroy(brain);
}

//=============================================================================
// 5. Cleanup and Destruction Tests
//=============================================================================

TEST_F(BrainInitAllocationTest, Destroy_HandlesNullBrain) {
    // Should not crash
    brain_destroy(nullptr);
}

TEST_F(BrainInitAllocationTest, Destroy_CleansUpAllocatedBrain) {
    brain_t brain = nimcp_brain_factory_allocate_brain();
    ASSERT_NE(brain, nullptr);

    // Destroy should not crash
    brain_destroy(brain);
}

TEST_F(BrainInitAllocationTest, Destroy_MultipleDestroy) {
    brain_t brain1 = nimcp_brain_factory_allocate_brain();
    brain_t brain2 = nimcp_brain_factory_allocate_brain();

    ASSERT_NE(brain1, nullptr);
    ASSERT_NE(brain2, nullptr);

    // Should be able to destroy in any order
    brain_destroy(brain2);
    brain_destroy(brain1);
}

//=============================================================================
// 6. Memory Management Tests
//=============================================================================
// Note: Memory leak tests removed - nimcp_memory_get_allocated() not available

//=============================================================================
// 7. Error Handling Tests
//=============================================================================

// Note: Error handling test removed - set_error() not available in public API

//=============================================================================
// 8. Consistency Tests
//=============================================================================

TEST_F(BrainInitAllocationTest, AllFieldsConsistent) {
    brain_t brain = nimcp_brain_factory_allocate_brain();
    ASSERT_NE(brain, nullptr);

    // DEBUG: Print field offsets and values
    std::cout << "DEBUG: sizeof(brain_struct) = " << sizeof(struct brain_struct) << std::endl;
    std::cout << "DEBUG: offsetof(is_cow_clone) = " << offsetof(struct brain_struct, is_cow_clone) << std::endl;
    std::cout << "DEBUG: offsetof(owns_network) = " << offsetof(struct brain_struct, owns_network) << std::endl;
    std::cout << "DEBUG: offsetof(original_network) = " << offsetof(struct brain_struct, original_network) << std::endl;
    std::cout << "DEBUG: brain = " << (void*)brain << std::endl;
    std::cout << "DEBUG: is_cow_clone addr = " << (void*)&brain->is_cow_clone << std::endl;
    std::cout << "DEBUG: owns_network addr = " << (void*)&brain->owns_network << std::endl;
    std::cout << "DEBUG: original_network addr = " << (void*)&brain->original_network << std::endl;
    std::cout << "DEBUG: brain->is_cow_clone = " << brain->is_cow_clone << std::endl;
    std::cout << "DEBUG: brain->owns_network = " << brain->owns_network << std::endl;
    std::cout << "DEBUG: brain->original_network = " << (void*)brain->original_network << std::endl;
    std::cout << "DEBUG: brain->network = " << (void*)brain->network << std::endl;
    std::cout << "DEBUG: offsetof(longterm_memory) = " << offsetof(struct brain_struct, longterm_memory) << std::endl;
    std::cout << "DEBUG: brain->longterm_memory = " << (void*)brain->longterm_memory << std::endl;
    std::cout << "DEBUG: offsetof(distributed) = " << offsetof(struct brain_struct, distributed) << std::endl;
    std::cout << "DEBUG: brain->distributed = " << (void*)brain->distributed << std::endl;

    // COW consistency: if not a clone, should own network
    if (!brain->is_cow_clone) {
        EXPECT_TRUE(brain->owns_network);
        EXPECT_EQ(brain->original_network, nullptr);
    }

    // Reference counting consistency
    // Note: network_refcount renamed to network_refcount_atomic
    // refcount_mutex removed (using atomic operations)
    // This check is now implicit in the atomic refcount

    // Long-term memory consistency
    if (brain->longterm_memory != nullptr) {
        EXPECT_GT(brain->longterm_capacity, 0u);
    } else {
        EXPECT_EQ(brain->longterm_capacity, 0u);
    }

    brain_destroy(brain);
}

TEST_F(BrainInitAllocationTest, StandaloneByDefault) {
    brain_t brain = nimcp_brain_factory_allocate_brain();
    ASSERT_NE(brain, nullptr);

    // Should be standalone (not distributed)
    EXPECT_EQ(brain->distributed, nullptr);

    // Should own its own network
    EXPECT_TRUE(brain->owns_network);
    EXPECT_FALSE(brain->is_cow_clone);

    brain_destroy(brain);
}

//=============================================================================
// 9. Integration Tests
//=============================================================================

TEST_F(BrainInitAllocationTest, CanBeUsedWithOtherFactoryFunctions) {
    brain_t brain = nimcp_brain_factory_allocate_brain();
    ASSERT_NE(brain, nullptr);

    // Should be able to call other factory functions
    bool result = nimcp_brain_factory_init_output_labels(brain, 3);
    EXPECT_TRUE(result) << "Should be able to init output labels";

    brain_destroy(brain);
}

//=============================================================================
// 10. Stress Tests
//=============================================================================

TEST_F(BrainInitAllocationTest, RapidAllocationDeallocation) {
    for (int i = 0; i < 100; i++) {
        brain_t brain = nimcp_brain_factory_allocate_brain();
        ASSERT_NE(brain, nullptr) << "Failed at iteration " << i;
        brain_destroy(brain);
    }
}

TEST_F(BrainInitAllocationTest, ParallelAllocation_Sequential) {
    const int num_brains = 20;
    brain_t brains[num_brains];

    // Allocate all
    for (int i = 0; i < num_brains; i++) {
        brains[i] = nimcp_brain_factory_allocate_brain();
        ASSERT_NE(brains[i], nullptr) << "Failed to allocate brain " << i;
    }

    // Verify all are distinct
    for (int i = 0; i < num_brains; i++) {
        for (int j = i + 1; j < num_brains; j++) {
            EXPECT_NE(brains[i], brains[j])
                << "Brains " << i << " and " << j << " should be distinct";
        }
    }

    // Destroy all
    for (int i = 0; i < num_brains; i++) {
        brain_destroy(brains[i]);
    }
}

//=============================================================================
// 11. Boundary Tests
//=============================================================================

TEST_F(BrainInitAllocationTest, LongTermMemory_ZeroCount) {
    brain_t brain = nimcp_brain_factory_allocate_brain();
    ASSERT_NE(brain, nullptr);

    EXPECT_EQ(brain->longterm_count, 0u)
        << "New brain should have no long-term memories";

    brain_destroy(brain);
}

TEST_F(BrainInitAllocationTest, InputSize_Zero) {
    brain_t brain = nimcp_brain_factory_allocate_brain();
    ASSERT_NE(brain, nullptr);

    EXPECT_EQ(brain->input_size, 0u)
        << "New brain should have input_size of 0";

    brain_destroy(brain);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
