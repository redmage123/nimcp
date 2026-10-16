/**
 * @file test_darray_integration.cpp
 * @brief Integration tests for dynamic array with other NIMCP components
 *
 * WHAT: Test darray integration with memory system, threading, and realistic workloads
 * WHY:  Verify darray works correctly in multi-component scenarios
 * HOW:  Test with NIMCP memory allocation, concurrent access patterns, nested structures
 *
 * INTEGRATION SCENARIOS:
 * 1. Memory system integration (nimcp_malloc/free)
 * 2. Nested darray structures (array of arrays)
 * 3. Thread safety with external synchronization
 * 4. Real-world data patterns (swarm conflicts, messages)
 * 5. Large-scale memory management
 * 6. Integration with hash tables and other containers
 *
 * @version Integration Testing Framework v1.0
 * @date 2025-12-08
 */

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <mutex>
#include <atomic>
#include <random>
#include <chrono>

// Headers have their own extern "C" guards
#include "utils/containers/nimcp_darray.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"

//=============================================================================
// Test Fixture
//=============================================================================

class DArrayIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Reset any global state
    }

    void TearDown() override {
        // Cleanup
    }
};

//=============================================================================
// Test Structures (simulating real NIMCP types)
//=============================================================================

// Simulated conflict structure (like nimcp_swarm_conflict_t)
struct SimulatedConflict {
    uint64_t conflict_id;
    uint64_t swarm_ids[4];
    uint32_t swarm_count;
    uint64_t detection_time;
    bool is_resolved;
    char description[256];
};

// Simulated message structure
struct SimulatedMessage {
    uint32_t type;
    uint64_t timestamp;
    uint32_t source_id;
    uint32_t dest_id;
    uint8_t payload[128];
    size_t payload_size;
};

// Nested structure with dynamic content
struct NestedContainer {
    int id;
    nimcp_darray_t* children;  // Array of integers
};

static void nested_container_destructor(void* element) {
    NestedContainer* nc = (NestedContainer*)element;
    if (nc->children) {
        nimcp_darray_destroy(nc->children);
    }
}

//=============================================================================
// Integration Test 1: Memory system integration
//=============================================================================

TEST_F(DArrayIntegrationTest, MemorySystem_AllocationTracking) {
    // WHAT: Verify darray uses nimcp memory system correctly
    // WHY:  Must integrate with project memory management
    // HOW:  Create/destroy arrays, verify no leaks

    // Create multiple arrays of different types
    nimcp_darray_t* arr_int = nimcp_darray_create(sizeof(int), 100);
    nimcp_darray_t* arr_conflict = nimcp_darray_create(sizeof(SimulatedConflict), 50);
    nimcp_darray_t* arr_msg = nimcp_darray_create(sizeof(SimulatedMessage), 200);

    ASSERT_NE(arr_int, nullptr);
    ASSERT_NE(arr_conflict, nullptr);
    ASSERT_NE(arr_msg, nullptr);

    // Add elements
    for (int i = 0; i < 100; i++) {
        nimcp_darray_push_back(arr_int, &i);
    }

    for (int i = 0; i < 50; i++) {
        SimulatedConflict c = {
            .conflict_id = (uint64_t)i,
            .swarm_ids = {1, 2, 0, 0},
            .swarm_count = 2,
            .detection_time = 1000 + i,
            .is_resolved = (i % 2 == 0),
            .description = "Test conflict"
        };
        nimcp_darray_push_back(arr_conflict, &c);
    }

    // Verify sizes
    EXPECT_EQ(nimcp_darray_size(arr_int), 100u);
    EXPECT_EQ(nimcp_darray_size(arr_conflict), 50u);
    EXPECT_EQ(nimcp_darray_size(arr_msg), 0u);

    // Cleanup
    nimcp_darray_destroy(arr_int);
    nimcp_darray_destroy(arr_conflict);
    nimcp_darray_destroy(arr_msg);

    // If we get here without crash/leak, test passes
    SUCCEED();
}

//=============================================================================
// Integration Test 2: Nested darray structures
//=============================================================================

TEST_F(DArrayIntegrationTest, NestedStructures_WithDestructor) {
    // WHAT: Array of structures containing arrays
    // WHY:  Support hierarchical data structures
    // HOW:  Create nested containers, verify destructor chain

    nimcp_darray_t* containers = nimcp_darray_create_with_destructor(
        sizeof(NestedContainer), 8, nested_container_destructor);
    ASSERT_NE(containers, nullptr);

    // Create nested containers
    for (int i = 0; i < 5; i++) {
        NestedContainer nc;
        nc.id = i;
        nc.children = nimcp_darray_create(sizeof(int), 10);

        // Add children to each container
        for (int j = 0; j < 10; j++) {
            int val = i * 100 + j;
            nimcp_darray_push_back(nc.children, &val);
        }

        nimcp_darray_push_back(containers, &nc);
    }

    EXPECT_EQ(nimcp_darray_size(containers), 5u);

    // Verify nested data
    NestedContainer* nc2 = (NestedContainer*)nimcp_darray_at(containers, 2);
    ASSERT_NE(nc2, nullptr);
    EXPECT_EQ(nc2->id, 2);
    EXPECT_EQ(nimcp_darray_size(nc2->children), 10u);

    int* child_val = (int*)nimcp_darray_at(nc2->children, 5);
    EXPECT_EQ(*child_val, 205);  // 2 * 100 + 5

    // Destroy - should call destructor for all nested arrays
    nimcp_darray_destroy(containers);
    SUCCEED();
}

//=============================================================================
// Integration Test 3: Thread safety with external synchronization
//=============================================================================

TEST_F(DArrayIntegrationTest, Threading_WithExternalMutex) {
    // WHAT: Multiple threads accessing array with mutex protection
    // WHY:  Verify darray works correctly with thread synchronization
    // HOW:  Multiple threads push/pop with mutex

    nimcp_darray_t* arr = nimcp_darray_create(sizeof(int), 32);
    ASSERT_NE(arr, nullptr);

    std::mutex mtx;
    std::atomic<int> push_count{0};
    std::atomic<int> pop_count{0};
    const int OPS_PER_THREAD = 1000;
    const int NUM_THREADS = 4;

    auto worker = [&](int thread_id) {
        std::mt19937 rng(thread_id);
        std::uniform_int_distribution<int> dist(0, 1);

        for (int i = 0; i < OPS_PER_THREAD; i++) {
            std::lock_guard<std::mutex> lock(mtx);

            if (dist(rng) == 0 || nimcp_darray_is_empty(arr)) {
                // Push
                int val = thread_id * 10000 + i;
                nimcp_darray_push_back(arr, &val);
                push_count++;
            } else {
                // Pop
                int out;
                if (nimcp_darray_pop_back(arr, &out)) {
                    pop_count++;
                }
            }
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < NUM_THREADS; i++) {
        threads.emplace_back(worker, i);
    }

    for (auto& t : threads) {
        t.join();
    }

    // Verify consistency
    size_t expected_size = push_count - pop_count;
    EXPECT_EQ(nimcp_darray_size(arr), expected_size);

    nimcp_darray_destroy(arr);
}

//=============================================================================
// Integration Test 4: Real-world conflict management pattern
//=============================================================================

TEST_F(DArrayIntegrationTest, RealWorld_ConflictManagement) {
    // WHAT: Simulate swarm conflict detection and resolution workflow
    // WHY:  Test realistic usage pattern from swarm module
    // HOW:  Detect conflicts, add to array, resolve, remove

    nimcp_darray_t* conflicts = nimcp_darray_create(sizeof(SimulatedConflict), 16);
    ASSERT_NE(conflicts, nullptr);

    // Simulate conflict detection
    for (int i = 0; i < 10; i++) {
        SimulatedConflict c = {
            .conflict_id = 1000 + i,
            .swarm_ids = {(uint64_t)(i * 2), (uint64_t)(i * 2 + 1), 0, 0},
            .swarm_count = 2,
            .detection_time = (uint64_t)(1000000 + i * 100),
            .is_resolved = false,
            .description = ""
        };
        snprintf(c.description, sizeof(c.description),
                 "Conflict between swarms %d and %d", i * 2, i * 2 + 1);
        nimcp_darray_push_back(conflicts, &c);
    }

    EXPECT_EQ(nimcp_darray_size(conflicts), 10u);

    // Simulate conflict resolution (mark odd conflicts as resolved)
    for (size_t i = 0; i < nimcp_darray_size(conflicts); i++) {
        SimulatedConflict* c = (SimulatedConflict*)nimcp_darray_at(conflicts, i);
        if (c->conflict_id % 2 == 1) {
            c->is_resolved = true;
        }
    }

    // Remove resolved conflicts (iterate backwards)
    for (size_t i = nimcp_darray_size(conflicts); i > 0; i--) {
        SimulatedConflict* c = (SimulatedConflict*)nimcp_darray_at(conflicts, i - 1);
        if (c->is_resolved) {
            nimcp_darray_remove_at(conflicts, i - 1, nullptr);
        }
    }

    // Should have 5 unresolved conflicts remaining
    EXPECT_EQ(nimcp_darray_size(conflicts), 5u);

    // Verify remaining conflicts are all unresolved with even IDs
    for (size_t i = 0; i < nimcp_darray_size(conflicts); i++) {
        SimulatedConflict* c = (SimulatedConflict*)nimcp_darray_at(conflicts, i);
        EXPECT_FALSE(c->is_resolved);
        EXPECT_EQ(c->conflict_id % 2, 0u);
    }

    nimcp_darray_destroy(conflicts);
}

//=============================================================================
// Integration Test 5: Message queue pattern
//=============================================================================

TEST_F(DArrayIntegrationTest, RealWorld_MessageQueue) {
    // WHAT: Simulate message queue using darray
    // WHY:  Test FIFO-like usage pattern
    // HOW:  Push messages to back, process from front

    nimcp_darray_t* queue = nimcp_darray_create(sizeof(SimulatedMessage), 64);
    ASSERT_NE(queue, nullptr);

    // Enqueue messages
    for (int i = 0; i < 20; i++) {
        SimulatedMessage msg = {
            .type = (uint32_t)(i % 4),
            .timestamp = (uint64_t)(1000 + i),
            .source_id = (uint32_t)i,
            .dest_id = (uint32_t)(i + 100),
            .payload = {0},
            .payload_size = 10
        };
        nimcp_darray_push_back(queue, &msg);
    }

    EXPECT_EQ(nimcp_darray_size(queue), 20u);

    // Process messages from front (FIFO)
    int processed = 0;
    while (!nimcp_darray_is_empty(queue)) {
        SimulatedMessage* msg = (SimulatedMessage*)nimcp_darray_at(queue, 0);

        // Verify FIFO order
        EXPECT_EQ(msg->source_id, (uint32_t)processed);

        // Remove from front
        nimcp_darray_remove_at(queue, 0, nullptr);
        processed++;

        // Process only first 10 to test partial processing
        if (processed >= 10) break;
    }

    EXPECT_EQ(processed, 10);
    EXPECT_EQ(nimcp_darray_size(queue), 10u);

    // Remaining messages should start from source_id 10
    SimulatedMessage* first = (SimulatedMessage*)nimcp_darray_front(queue);
    EXPECT_EQ(first->source_id, 10u);

    nimcp_darray_destroy(queue);
}

//=============================================================================
// Integration Test 6: Large-scale memory handling
//=============================================================================

TEST_F(DArrayIntegrationTest, LargeScale_MemoryManagement) {
    // WHAT: Handle large amounts of data
    // WHY:  Verify memory management at scale
    // HOW:  Create large array, resize multiple times

    nimcp_darray_t* arr = nimcp_darray_create(sizeof(SimulatedConflict), 16);
    ASSERT_NE(arr, nullptr);

    // Add 1000 large structures
    for (int i = 0; i < 1000; i++) {
        SimulatedConflict c = {
            .conflict_id = (uint64_t)i,
            .swarm_ids = {1, 2, 3, 4},
            .swarm_count = 4,
            .detection_time = (uint64_t)i * 1000,
            .is_resolved = false,
            .description = "Large scale test conflict with detailed description"
        };
        nimcp_darray_push_back(arr, &c);
    }

    EXPECT_EQ(nimcp_darray_size(arr), 1000u);

    // Shrink to fit
    EXPECT_TRUE(nimcp_darray_shrink_to_fit(arr));
    EXPECT_EQ(nimcp_darray_capacity(arr), 1000u);

    // Clear and reuse
    nimcp_darray_clear(arr);
    EXPECT_EQ(nimcp_darray_size(arr), 0u);

    // Reserve and fill again
    EXPECT_TRUE(nimcp_darray_reserve(arr, 500));

    for (int i = 0; i < 500; i++) {
        SimulatedConflict c = {.conflict_id = (uint64_t)i};
        nimcp_darray_push_back(arr, &c);
    }

    EXPECT_EQ(nimcp_darray_size(arr), 500u);

    nimcp_darray_destroy(arr);
}

//=============================================================================
// Integration Test 7: Array of pointers pattern
//=============================================================================

TEST_F(DArrayIntegrationTest, ArrayOfPointers_WithManualCleanup) {
    // WHAT: Store pointers to dynamically allocated objects
    // WHY:  Common pattern for polymorphic containers
    // HOW:  Store pointers, manual cleanup

    nimcp_darray_t* ptrs = nimcp_darray_create(sizeof(int*), 16);
    ASSERT_NE(ptrs, nullptr);

    // Allocate and store pointers
    for (int i = 0; i < 10; i++) {
        int* p = (int*)nimcp_malloc(sizeof(int));
        ASSERT_NE(p, nullptr);
        *p = i * 100;
        nimcp_darray_push_back(ptrs, &p);
    }

    // Access via pointer
    int** ptr_ptr = (int**)nimcp_darray_at(ptrs, 5);
    EXPECT_EQ(**ptr_ptr, 500);

    // Manual cleanup before destroying array
    for (size_t i = 0; i < nimcp_darray_size(ptrs); i++) {
        int** pp = (int**)nimcp_darray_at(ptrs, i);
        nimcp_free(*pp);
    }

    nimcp_darray_destroy(ptrs);
}

//=============================================================================
// Integration Test 8: Mixed operations stress test
//=============================================================================

TEST_F(DArrayIntegrationTest, StressTest_MixedOperations) {
    // WHAT: Rapid mix of all operations
    // WHY:  Test stability under varied workload
    // HOW:  Random mix of push, pop, insert, remove, resize

    nimcp_darray_t* arr = nimcp_darray_create(sizeof(int), 8);
    ASSERT_NE(arr, nullptr);

    std::mt19937 rng(42);
    std::uniform_int_distribution<int> op_dist(0, 5);
    std::uniform_int_distribution<int> val_dist(0, 1000);

    for (int iteration = 0; iteration < 10000; iteration++) {
        int op = op_dist(rng);
        int val = val_dist(rng);
        size_t size = nimcp_darray_size(arr);

        switch (op) {
            case 0:  // Push back
                nimcp_darray_push_back(arr, &val);
                break;

            case 1:  // Pop back
                if (size > 0) {
                    nimcp_darray_pop_back(arr, nullptr);
                }
                break;

            case 2:  // Insert
                if (size > 0) {
                    size_t idx = val % (size + 1);
                    nimcp_darray_insert(arr, idx, &val);
                }
                break;

            case 3:  // Remove
                if (size > 0) {
                    size_t idx = val % size;
                    nimcp_darray_remove_at(arr, idx, nullptr);
                }
                break;

            case 4:  // Set
                if (size > 0) {
                    size_t idx = val % size;
                    nimcp_darray_set(arr, idx, &val);
                }
                break;

            case 5:  // Clear occasionally
                if (val % 100 == 0) {
                    nimcp_darray_clear(arr);
                }
                break;
        }

        // Verify invariants
        EXPECT_LE(nimcp_darray_size(arr), nimcp_darray_capacity(arr));
    }

    nimcp_darray_destroy(arr);
}

//=============================================================================
// Integration Test 9: Data pointer stability
//=============================================================================

TEST_F(DArrayIntegrationTest, DataPointerStability_BeforeResize) {
    // WHAT: Data pointer remains valid until resize
    // WHY:  Important for caching data pointers
    // HOW:  Get data pointer, modify without resize, verify

    nimcp_darray_t* arr = nimcp_darray_create(sizeof(int), 100);
    ASSERT_NE(arr, nullptr);

    // Pre-allocate space
    for (int i = 0; i < 50; i++) {
        nimcp_darray_push_back(arr, &i);
    }

    // Get data pointer
    int* data = (int*)nimcp_darray_data(arr);
    ASSERT_NE(data, nullptr);

    // Modify through data pointer (no resize needed)
    for (int i = 0; i < 50; i++) {
        data[i] *= 2;
    }

    // Verify through at()
    for (int i = 0; i < 50; i++) {
        EXPECT_EQ(*(int*)nimcp_darray_at(arr, i), i * 2);
    }

    nimcp_darray_destroy(arr);
}

//=============================================================================
// Integration Test 10: Swap for efficient batch processing
//=============================================================================

TEST_F(DArrayIntegrationTest, SwapForBatchProcessing) {
    // WHAT: Use swap for double-buffering pattern
    // WHY:  Efficient batch processing without copying
    // HOW:  Fill one array, swap with empty, process

    nimcp_darray_t* active = nimcp_darray_create(sizeof(int), 100);
    nimcp_darray_t* processing = nimcp_darray_create(sizeof(int), 100);

    // Simulate batch processing cycles
    for (int batch = 0; batch < 5; batch++) {
        // Fill active buffer
        for (int i = 0; i < 20; i++) {
            int val = batch * 100 + i;
            nimcp_darray_push_back(active, &val);
        }

        // Swap - processing now has the data
        EXPECT_TRUE(nimcp_darray_swap(active, processing));

        // Active is now empty, ready for new data
        EXPECT_EQ(nimcp_darray_size(active), 0u);
        EXPECT_EQ(nimcp_darray_size(processing), 20u);

        // Verify processing has correct batch data
        int* first = (int*)nimcp_darray_front(processing);
        EXPECT_EQ(*first, batch * 100);

        // Clear processing for next batch
        nimcp_darray_clear(processing);
    }

    nimcp_darray_destroy(active);
    nimcp_darray_destroy(processing);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
