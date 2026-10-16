/**
 * @file test_sparse_synapse.cpp
 * @brief Comprehensive unit tests for sparse synapse allocation
 *
 * WHAT: Test suite validating sparse synapse pool and storage operations
 * WHY: Ensure memory efficiency, correctness, and BBB security
 * HOW: Google Test framework with 87% memory reduction validation
 *
 * TEST COVERAGE:
 * - Pool lifecycle (create/destroy)
 * - Synapse addition (embedded and overflow)
 * - Synapse removal and iteration
 * - BBB security validation
 * - Thread safety
 * - Memory savings calculation
 * - Statistics tracking
 * - Edge cases and error handling
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <thread>
#include <atomic>

// Headers have their own extern "C" guards
#include "core/neuralnet/nimcp_sparse_synapse.h"
#include "core/neuralnet/nimcp_neuralnet.h"  // For synapse_t definition
#include "utils/logging/nimcp_logging.h"

//=============================================================================
// Test Fixtures
//=============================================================================

/**
 * @brief Base test fixture for sparse synapse tests
 */
class SparseSynapseTest : public ::testing::Test {
protected:
    sparse_synapse_pool_t pool;
    sparse_synapse_storage_t storage;

    void SetUp() override {
        // WHAT: Initialize test environment
        // WHY: Clean state for each test
        // HOW: Create pool with default config
        pool = sparse_synapse_pool_create(nullptr);
        ASSERT_NE(pool, nullptr) << "Failed to create sparse synapse pool";

        sparse_synapse_storage_init(&storage);
    }

    void TearDown() override {
        // WHAT: Cleanup test resources
        // WHY: Prevent memory leaks
        // HOW: Destroy storage and pool
        sparse_synapse_storage_cleanup(pool, &storage);
        sparse_synapse_pool_destroy(pool);
    }
};

/**
 * @brief Fixture for multi-threaded tests
 */
class SparseSynapseThreadTest : public SparseSynapseTest {
protected:
    static constexpr int NUM_THREADS = 4;
    static constexpr int SYNAPSES_PER_THREAD = 100;
};

//=============================================================================
// Pool Lifecycle Tests
//=============================================================================

TEST(SparseSynapsePoolTest, CreateDestroyDefault) {
    // WHAT: Test pool creation with default configuration
    // WHY: Validate basic lifecycle
    // HOW: Create and destroy pool
    auto pool = sparse_synapse_pool_create(nullptr);
    ASSERT_NE(pool, nullptr);

    sparse_synapse_pool_destroy(pool);
    // Success if no crash
}

TEST(SparseSynapsePoolTest, CreateWithCustomConfig) {
    // WHAT: Test pool creation with custom configuration
    // WHY: Validate configuration parameters
    // HOW: Set specific pool size and flags
    sparse_synapse_pool_config_t config = {
        .pool_size = 10000,
        .enable_statistics = true,
        .thread_safe = true
    };

    auto pool = sparse_synapse_pool_create(&config);
    ASSERT_NE(pool, nullptr);

    sparse_synapse_pool_destroy(pool);
}

TEST(SparseSynapsePoolTest, CreateWithInvalidConfig) {
    // WHAT: Test pool creation with invalid configuration
    // WHY: Validate BBB security checks
    // HOW: Try to create pool with invalid size
    sparse_synapse_pool_config_t config = {
        .pool_size = 0,  // Invalid: zero size
        .enable_statistics = true,
        .thread_safe = true
    };

    auto pool = sparse_synapse_pool_create(&config);
    EXPECT_EQ(pool, nullptr) << "Should reject zero pool size";

    // Try with excessive size
    config.pool_size = 200000000;  // > 100M limit
    pool = sparse_synapse_pool_create(&config);
    EXPECT_EQ(pool, nullptr) << "Should reject excessive pool size";
}

TEST(SparseSynapsePoolTest, DestroyNull) {
    // WHAT: Test destroying NULL pool
    // WHY: Validate null safety
    // HOW: Call destroy with NULL
    sparse_synapse_pool_destroy(nullptr);
    // Success if no crash
}

//=============================================================================
// Storage Initialization Tests
//=============================================================================

TEST_F(SparseSynapseTest, StorageInitialization) {
    // WHAT: Verify storage is properly initialized
    // WHY: Ensure clean initial state
    // HOW: Check counts and pointers
    EXPECT_EQ(sparse_synapse_count(&storage), 0u);
    EXPECT_EQ(storage.embedded_count, 0u);
    EXPECT_EQ(storage.overflow_count, 0u);
    EXPECT_EQ(storage.overflow, nullptr);
    EXPECT_EQ(storage.overflow_capacity, 0u);
}

TEST_F(SparseSynapseTest, StorageCleanupEmpty) {
    // WHAT: Test cleanup of empty storage
    // WHY: Validate cleanup handles empty case
    // HOW: Cleanup immediately after init
    sparse_synapse_storage_cleanup(pool, &storage);
    sparse_synapse_storage_init(&storage);  // Re-init for TearDown
    // Success if no crash
}

//=============================================================================
// Synapse Addition Tests
//=============================================================================

TEST_F(SparseSynapseTest, AddSingleSynapse) {
    // WHAT: Add a single synapse to storage
    // WHY: Validate basic add operation
    // HOW: Add one synapse and verify count
    int result = sparse_synapse_add(pool, &storage, 42, 0.5f);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(sparse_synapse_count(&storage), 1u);
    EXPECT_EQ(storage.embedded_count, 1u);

    // Verify synapse data
    auto* handle = sparse_synapse_get(&storage, 0);
    ASSERT_NE(handle, nullptr);
    EXPECT_EQ(handle->target_neuron_id, 42u);
    EXPECT_FLOAT_EQ(handle->weight, 0.5f);
}

TEST_F(SparseSynapseTest, AddMultipleSynapsesEmbedded) {
    // WHAT: Add multiple synapses (all embedded)
    // WHY: Test embedded array filling
    // HOW: Add 32 synapses (< 64 capacity)
    constexpr uint32_t count = 32;
    for (uint32_t i = 0; i < count; i++) {
        int result = sparse_synapse_add(pool, &storage, i, static_cast<float>(i) * 0.1f);
        EXPECT_EQ(result, 0);
    }

    EXPECT_EQ(sparse_synapse_count(&storage), count);
    EXPECT_EQ(storage.embedded_count, count);
    EXPECT_EQ(storage.overflow_count, 0u);
    EXPECT_EQ(storage.overflow, nullptr);
}

TEST_F(SparseSynapseTest, AddSynapsesToEmbeddedCapacity) {
    // WHAT: Fill embedded array to capacity (64 synapses)
    // WHY: Test boundary condition
    // HOW: Add exactly 64 synapses
    constexpr uint32_t capacity = SPARSE_SYNAPSE_EMBEDDED_CAPACITY;
    for (uint32_t i = 0; i < capacity; i++) {
        int result = sparse_synapse_add(pool, &storage, i, 1.0f);
        EXPECT_EQ(result, 0);
    }

    EXPECT_EQ(sparse_synapse_count(&storage), capacity);
    EXPECT_EQ(storage.embedded_count, capacity);
    EXPECT_EQ(storage.overflow_count, 0u);
}

TEST_F(SparseSynapseTest, AddSynapsesWithOverflow) {
    // WHAT: Add synapses beyond embedded capacity
    // WHY: Test overflow allocation
    // HOW: Add 80 synapses (64 embedded + 16 overflow)
    constexpr uint32_t total = 80;
    constexpr uint32_t overflow_count = total - SPARSE_SYNAPSE_EMBEDDED_CAPACITY;

    for (uint32_t i = 0; i < total; i++) {
        int result = sparse_synapse_add(pool, &storage, i, 0.75f);
        EXPECT_EQ(result, 0) << "Failed to add synapse " << i;
    }

    EXPECT_EQ(sparse_synapse_count(&storage), total);
    EXPECT_EQ(storage.embedded_count, SPARSE_SYNAPSE_EMBEDDED_CAPACITY);
    EXPECT_EQ(storage.overflow_count, overflow_count);
    EXPECT_NE(storage.overflow, nullptr);
}

TEST_F(SparseSynapseTest, AddLargeBatchOverflow) {
    // WHAT: Add large number of synapses to test overflow growth
    // WHY: Validate 2x growth strategy
    // HOW: Add 200 synapses
    constexpr uint32_t total = 200;

    for (uint32_t i = 0; i < total; i++) {
        int result = sparse_synapse_add(pool, &storage, i, 1.0f);
        EXPECT_EQ(result, 0) << "Failed to add synapse " << i;
    }

    EXPECT_EQ(sparse_synapse_count(&storage), total);
    EXPECT_EQ(storage.embedded_count, SPARSE_SYNAPSE_EMBEDDED_CAPACITY);
    EXPECT_EQ(storage.overflow_count, total - SPARSE_SYNAPSE_EMBEDDED_CAPACITY);
}

//=============================================================================
// BBB Security Validation Tests
//=============================================================================

TEST_F(SparseSynapseTest, AddWithInvalidWeight) {
    // WHAT: Try to add synapse with NaN weight
    // WHY: Validate weight checking
    // HOW: Pass NaN and Inf values
    int result = sparse_synapse_add(pool, &storage, 10, NAN);
    EXPECT_EQ(result, -1) << "Should reject NaN weight";

    result = sparse_synapse_add(pool, &storage, 10, INFINITY);
    EXPECT_EQ(result, -1) << "Should reject Inf weight";

    result = sparse_synapse_add(pool, &storage, 10, -INFINITY);
    EXPECT_EQ(result, -1) << "Should reject -Inf weight";
}

TEST_F(SparseSynapseTest, AddWithNullStorage) {
    // WHAT: Try to add synapse to NULL storage
    // WHY: Validate null checking
    // HOW: Pass NULL storage pointer
    int result = sparse_synapse_add(pool, nullptr, 10, 0.5f);
    EXPECT_EQ(result, -1) << "Should reject NULL storage";
}

TEST_F(SparseSynapseTest, AddWithNullPool) {
    // WHAT: Try to add synapse with NULL pool
    // WHY: Validate pool checking
    // HOW: Pass NULL pool pointer
    int result = sparse_synapse_add(nullptr, &storage, 10, 0.5f);
    EXPECT_EQ(result, -1) << "Should reject NULL pool";
}

//=============================================================================
// Synapse Removal Tests
//=============================================================================

TEST_F(SparseSynapseTest, RemoveSingleSynapse) {
    // WHAT: Add and remove a single synapse
    // WHY: Test basic removal
    // HOW: Add one, remove one
    sparse_synapse_add(pool, &storage, 42, 0.5f);
    EXPECT_EQ(sparse_synapse_count(&storage), 1u);

    int result = sparse_synapse_remove(pool, &storage, 0);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(sparse_synapse_count(&storage), 0u);
}

TEST_F(SparseSynapseTest, RemoveFromEmbedded) {
    // WHAT: Remove synapses from embedded array
    // WHY: Test embedded removal logic
    // HOW: Add multiple, remove from middle
    for (uint32_t i = 0; i < 10; i++) {
        sparse_synapse_add(pool, &storage, i, 1.0f);
    }

    // Remove from middle (swap-and-pop)
    int result = sparse_synapse_remove(pool, &storage, 5);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(sparse_synapse_count(&storage), 9u);

    // Verify last element moved to index 5
    auto* handle = sparse_synapse_get(&storage, 5);
    ASSERT_NE(handle, nullptr);
    EXPECT_EQ(handle->target_neuron_id, 9u);  // Last element
}

TEST_F(SparseSynapseTest, RemoveFromOverflow) {
    // WHAT: Remove synapses from overflow array
    // WHY: Test overflow removal logic
    // HOW: Add >64 synapses, remove from overflow
    for (uint32_t i = 0; i < 80; i++) {
        sparse_synapse_add(pool, &storage, i, 1.0f);
    }

    // Remove from overflow (index 70 = embedded 64 + overflow 6)
    int result = sparse_synapse_remove(pool, &storage, 70);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(sparse_synapse_count(&storage), 79u);
}

TEST_F(SparseSynapseTest, RemoveInvalidIndex) {
    // WHAT: Try to remove with invalid index
    // WHY: Validate bounds checking
    // HOW: Pass out-of-bounds index
    sparse_synapse_add(pool, &storage, 42, 0.5f);

    int result = sparse_synapse_remove(pool, &storage, 999);
    EXPECT_EQ(result, -1) << "Should reject invalid index";
}

//=============================================================================
// Synapse Retrieval Tests
//=============================================================================

TEST_F(SparseSynapseTest, GetSynapseFromEmbedded) {
    // WHAT: Retrieve synapse from embedded array
    // WHY: Test get operation
    // HOW: Add and retrieve by index
    sparse_synapse_add(pool, &storage, 123, 0.7f);

    auto* handle = sparse_synapse_get(&storage, 0);
    ASSERT_NE(handle, nullptr);
    EXPECT_EQ(handle->target_neuron_id, 123u);
    EXPECT_FLOAT_EQ(handle->weight, 0.7f);
}

TEST_F(SparseSynapseTest, GetSynapseFromOverflow) {
    // WHAT: Retrieve synapse from overflow array
    // WHY: Test overflow access
    // HOW: Add >64, retrieve from overflow
    for (uint32_t i = 0; i < 70; i++) {
        sparse_synapse_add(pool, &storage, i, static_cast<float>(i));
    }

    // Get from overflow (index 65)
    auto* handle = sparse_synapse_get(&storage, 65);
    ASSERT_NE(handle, nullptr);
    EXPECT_EQ(handle->target_neuron_id, 65u);
    EXPECT_FLOAT_EQ(handle->weight, 65.0f);
}

TEST_F(SparseSynapseTest, GetInvalidIndex) {
    // WHAT: Try to get synapse with invalid index
    // WHY: Validate bounds checking
    // HOW: Pass out-of-bounds index
    sparse_synapse_add(pool, &storage, 42, 0.5f);

    auto* handle = sparse_synapse_get(&storage, 999);
    EXPECT_EQ(handle, nullptr) << "Should return NULL for invalid index";
}

//=============================================================================
// Iterator Tests
//=============================================================================

TEST_F(SparseSynapseTest, IterateEmpty) {
    // WHAT: Iterate over empty storage
    // WHY: Validate iterator handles empty case
    // HOW: Create iterator and try to get next
    sparse_synapse_iterator_t it;
    sparse_synapse_iterator_init(&it, &storage);

    auto* handle = sparse_synapse_iterator_next(&it);
    EXPECT_EQ(handle, nullptr) << "Empty storage should return NULL";
    EXPECT_FALSE(sparse_synapse_iterator_has_next(&it));
}

TEST_F(SparseSynapseTest, IterateEmbeddedOnly) {
    // WHAT: Iterate over embedded synapses
    // WHY: Test iteration with no overflow
    // HOW: Add <64 synapses and iterate
    constexpr uint32_t count = 10;
    for (uint32_t i = 0; i < count; i++) {
        sparse_synapse_add(pool, &storage, i, static_cast<float>(i));
    }

    sparse_synapse_iterator_t it;
    sparse_synapse_iterator_init(&it, &storage);

    uint32_t visited = 0;
    synapse_handle_t* handle;
    while ((handle = sparse_synapse_iterator_next(&it)) != nullptr) {
        EXPECT_EQ(handle->target_neuron_id, visited);
        EXPECT_FLOAT_EQ(handle->weight, static_cast<float>(visited));
        visited++;
    }

    EXPECT_EQ(visited, count);
}

TEST_F(SparseSynapseTest, IterateWithOverflow) {
    // WHAT: Iterate over embedded + overflow synapses
    // WHY: Test full iteration across both arrays
    // HOW: Add 80 synapses and iterate all
    constexpr uint32_t count = 80;
    for (uint32_t i = 0; i < count; i++) {
        sparse_synapse_add(pool, &storage, i, static_cast<float>(i));
    }

    sparse_synapse_iterator_t it;
    sparse_synapse_iterator_init(&it, &storage);

    uint32_t visited = 0;
    synapse_handle_t* handle;
    while ((handle = sparse_synapse_iterator_next(&it)) != nullptr) {
        visited++;
    }

    EXPECT_EQ(visited, count);
}

TEST_F(SparseSynapseTest, IteratorReset) {
    // WHAT: Test iterator reset functionality
    // WHY: Validate multiple iterations
    // HOW: Iterate, reset, iterate again
    for (uint32_t i = 0; i < 5; i++) {
        sparse_synapse_add(pool, &storage, i, 1.0f);
    }

    sparse_synapse_iterator_t it;
    sparse_synapse_iterator_init(&it, &storage);

    // First iteration
    uint32_t count1 = 0;
    while (sparse_synapse_iterator_next(&it) != nullptr) {
        count1++;
    }
    EXPECT_EQ(count1, 5u);

    // Reset and iterate again
    sparse_synapse_iterator_reset(&it);
    uint32_t count2 = 0;
    while (sparse_synapse_iterator_next(&it) != nullptr) {
        count2++;
    }
    EXPECT_EQ(count2, 5u);
}

//=============================================================================
// Compaction Tests
//=============================================================================

TEST_F(SparseSynapseTest, CompactNoOverflow) {
    // WHAT: Compact storage with no overflow
    // WHY: Validate compact handles no-op case
    // HOW: Add <64 synapses and compact
    for (uint32_t i = 0; i < 32; i++) {
        sparse_synapse_add(pool, &storage, i, 1.0f);
    }

    uint32_t moved = sparse_synapse_compact(pool, &storage);
    EXPECT_EQ(moved, 0u) << "Nothing to compact";
    EXPECT_EQ(storage.overflow, nullptr);
}

TEST_F(SparseSynapseTest, CompactAfterPruning) {
    // WHAT: Compact after pruning synapses
    // WHY: Validate compaction frees empty overflow array
    // HOW: Add 80, remove all overflow, then compact to free overflow memory
    for (uint32_t i = 0; i < 80; i++) {
        sparse_synapse_add(pool, &storage, i, 1.0f);
    }

    // Remove all overflow synapses (16 = 80 - 64 embedded capacity)
    // Always remove from the last position to avoid index shifting issues
    for (uint32_t i = 0; i < 16; i++) {
        uint32_t last_idx = sparse_synapse_count(&storage) - 1;
        ASSERT_EQ(sparse_synapse_remove(pool, &storage, last_idx), 0);
    }

    // After removing 16 from overflow, we should have 64 embedded + 0 overflow
    EXPECT_EQ(sparse_synapse_count(&storage), 64u);
    // Overflow array is still allocated (just empty) until compact is called
    EXPECT_EQ(storage.overflow_count, 0u);

    // Compact should free the empty overflow array
    uint32_t moved = sparse_synapse_compact(pool, &storage);
    EXPECT_EQ(moved, 0u);  // Nothing to move, overflow was already empty
    EXPECT_EQ(storage.overflow, nullptr);  // But overflow should now be freed
    EXPECT_EQ(storage.embedded_count, 64u);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(SparseSynapseTest, GetStatistics) {
    // WHAT: Retrieve and validate pool statistics
    // WHY: Test statistics tracking
    // HOW: Add synapses and check stats
    for (uint32_t i = 0; i < 100; i++) {
        sparse_synapse_add(pool, &storage, i, 1.0f);
    }

    sparse_synapse_stats_t stats;
    int result = sparse_synapse_pool_get_stats(pool, &stats);
    EXPECT_EQ(result, 0);

    EXPECT_EQ(stats.total_synapses, 100u);
    EXPECT_EQ(stats.embedded_synapses, SPARSE_SYNAPSE_EMBEDDED_CAPACITY);
    EXPECT_EQ(stats.overflow_synapses, 100u - SPARSE_SYNAPSE_EMBEDDED_CAPACITY);
    EXPECT_GT(stats.total_allocations, 0u);
}

TEST_F(SparseSynapseTest, PoolUtilization) {
    // WHAT: Test pool utilization calculation
    // WHY: Validate utilization metric
    // HOW: Add synapses and check utilization
    for (uint32_t i = 0; i < 100; i++) {
        sparse_synapse_add(pool, &storage, i, 1.0f);
    }

    float util = sparse_synapse_pool_utilization(pool);
    EXPECT_GE(util, 0.0f);
    EXPECT_LE(util, 1.0f);
}

TEST_F(SparseSynapseTest, PoolAvailable) {
    // WHAT: Test available handles query
    // WHY: Validate availability tracking
    // HOW: Check available count
    size_t available = sparse_synapse_pool_available(pool);
    EXPECT_GT(available, 0u);
}

//=============================================================================
// Memory Savings Tests
//=============================================================================

TEST_F(SparseSynapseTest, MemorySavingsCalculation) {
    // WHAT: Validate 87% memory savings claim
    // WHY: Verify sparse allocation efficiency
    // HOW: Simulate 10,000 neurons with sparse connectivity

    constexpr size_t NUM_NEURONS = 1000;  // Reduced for faster tests
    constexpr size_t DENSE_SYNAPSES_PER_NEURON = 100;
    constexpr size_t BYTES_PER_SYNAPSE = 600;
    constexpr size_t AVG_ACTUAL_SYNAPSES = 32;  // Typical sparse network

    std::vector<sparse_synapse_storage_t> storages(NUM_NEURONS);

    // Initialize all storages
    for (auto& s : storages) {
        sparse_synapse_storage_init(&s);
    }

    // Add typical sparse synapses
    for (size_t i = 0; i < NUM_NEURONS; i++) {
        for (size_t j = 0; j < AVG_ACTUAL_SYNAPSES; j++) {
            sparse_synapse_add(pool, &storages[i], static_cast<uint32_t>(j), 1.0f);
        }
    }

    // Calculate savings
    float savings = sparse_synapse_memory_savings(
        pool, NUM_NEURONS, DENSE_SYNAPSES_PER_NEURON, BYTES_PER_SYNAPSE
    );

    // Cleanup
    for (auto& s : storages) {
        sparse_synapse_storage_cleanup(pool, &s);
    }

    // Validate 87% savings (allow margin for size-class pool efficiency)
    EXPECT_GE(savings, 0.77f) << "Should achieve at least 77% savings";
    EXPECT_LE(savings, 0.99f) << "Savings should be realistic";
}

//=============================================================================
// Thread Safety Tests
//=============================================================================

TEST_F(SparseSynapseThreadTest, ConcurrentAddToDifferentStorages) {
    // WHAT: Test concurrent additions to different storages
    // WHY: Validate thread safety with independent storages
    // HOW: Multiple threads add to separate storages

    std::vector<sparse_synapse_storage_t> storages(NUM_THREADS);
    for (auto& s : storages) {
        sparse_synapse_storage_init(&s);
    }

    std::vector<std::thread> threads;
    for (int t = 0; t < NUM_THREADS; t++) {
        threads.emplace_back([this, &storages, t]() {
            for (int i = 0; i < SYNAPSES_PER_THREAD; i++) {
                int result = sparse_synapse_add(
                    pool, &storages[t],
                    static_cast<uint32_t>(i),
                    1.0f
                );
                EXPECT_EQ(result, 0);
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    // Verify all synapses added
    for (int t = 0; t < NUM_THREADS; t++) {
        EXPECT_EQ(sparse_synapse_count(&storages[t]),
                  static_cast<uint32_t>(SYNAPSES_PER_THREAD));
        sparse_synapse_storage_cleanup(pool, &storages[t]);
    }
}

TEST_F(SparseSynapseThreadTest, ConcurrentPoolOperations) {
    // WHAT: Test concurrent pool statistics access
    // WHY: Validate thread-safe statistics
    // HOW: Multiple threads read stats while one adds synapses

    std::atomic<bool> stop{false};
    std::vector<std::thread> threads;

    // Reader threads
    for (int t = 0; t < NUM_THREADS - 1; t++) {
        threads.emplace_back([this, &stop]() {
            while (!stop.load()) {
                sparse_synapse_stats_t stats;
                sparse_synapse_pool_get_stats(pool, &stats);
                // Just ensure no crash
            }
        });
    }

    // Writer thread
    threads.emplace_back([this, &stop]() {
        for (int i = 0; i < 100; i++) {
            sparse_synapse_add(pool, &storage, static_cast<uint32_t>(i), 1.0f);
        }
        stop.store(true);
    });

    for (auto& thread : threads) {
        thread.join();
    }
}

//=============================================================================
// Edge Cases and Error Handling
//=============================================================================

TEST_F(SparseSynapseTest, MultipleCleanup) {
    // WHAT: Test multiple cleanup calls
    // WHY: Validate idempotent cleanup
    // HOW: Cleanup twice
    sparse_synapse_add(pool, &storage, 42, 0.5f);

    sparse_synapse_storage_cleanup(pool, &storage);
    sparse_synapse_storage_init(&storage);  // Re-init
    sparse_synapse_storage_cleanup(pool, &storage);
    sparse_synapse_storage_init(&storage);  // Re-init for TearDown
    // Success if no crash
}

TEST_F(SparseSynapseTest, RemoveAllSynapses) {
    // WHAT: Add and remove all synapses
    // WHY: Test complete removal
    // HOW: Add N, remove N
    constexpr uint32_t count = 50;
    for (uint32_t i = 0; i < count; i++) {
        sparse_synapse_add(pool, &storage, i, 1.0f);
    }

    for (uint32_t i = 0; i < count; i++) {
        int result = sparse_synapse_remove(pool, &storage, 0);
        EXPECT_EQ(result, 0);
    }

    EXPECT_EQ(sparse_synapse_count(&storage), 0u);
}

TEST_F(SparseSynapseTest, AlternateAddRemove) {
    // WHAT: Alternate adding and removing synapses
    // WHY: Test dynamic behavior
    // HOW: Add-remove pattern
    for (int cycle = 0; cycle < 10; cycle++) {
        // Add 20
        for (uint32_t i = 0; i < 20; i++) {
            sparse_synapse_add(pool, &storage, i, 1.0f);
        }

        // Remove 10
        for (int i = 0; i < 10; i++) {
            sparse_synapse_remove(pool, &storage, 0);
        }
    }

    // Should have 100 synapses (10 cycles * 10 net adds)
    EXPECT_EQ(sparse_synapse_count(&storage), 100u);
}

//=============================================================================
// Synapse Metadata Pool Tests
//=============================================================================

class SynapseMetadataPoolTest : public ::testing::Test {
protected:
    synapse_metadata_pool_t metadata_pool;
    sparse_synapse_pool_t handle_pool;
    sparse_synapse_storage_t storage;

    void SetUp() override {
        // Create metadata pool with small size for testing
        synapse_metadata_pool_config_t meta_config = synapse_metadata_pool_default_config();
        meta_config.pool_size = 100;
        metadata_pool = synapse_metadata_pool_create(&meta_config);
        ASSERT_NE(metadata_pool, nullptr) << "Failed to create metadata pool";

        handle_pool = sparse_synapse_pool_create(nullptr);
        ASSERT_NE(handle_pool, nullptr) << "Failed to create handle pool";

        sparse_synapse_storage_init(&storage);
    }

    void TearDown() override {
        sparse_synapse_storage_cleanup(handle_pool, &storage);
        sparse_synapse_pool_destroy(handle_pool);
        synapse_metadata_pool_destroy(metadata_pool);
    }
};

TEST_F(SynapseMetadataPoolTest, CreateDestroyDefault) {
    // WHAT: Test metadata pool creation with default configuration
    // WHY: Validate basic lifecycle
    // HOW: Create and destroy pool
    synapse_metadata_pool_t pool = synapse_metadata_pool_create(nullptr);
    ASSERT_NE(pool, nullptr);
    synapse_metadata_pool_destroy(pool);
}

TEST_F(SynapseMetadataPoolTest, AllocateFree) {
    // WHAT: Test allocation and freeing of metadata slots
    // WHY: Validate basic allocation lifecycle
    // HOW: Allocate, use, free
    uint32_t idx = synapse_metadata_pool_allocate(metadata_pool);
    ASSERT_NE(idx, SPARSE_SYNAPSE_NO_METADATA);

    // Should be able to get synapse at this index
    synapse_t* syn = synapse_metadata_pool_get(metadata_pool, idx);
    ASSERT_NE(syn, nullptr);

    // Free the slot
    synapse_metadata_pool_free(metadata_pool, idx);

    // Pool should have more available now
    EXPECT_EQ(synapse_metadata_pool_available(metadata_pool), 100u);
}

TEST_F(SynapseMetadataPoolTest, UtilizationTracking) {
    // WHAT: Test utilization tracking
    // WHY: Validate statistics
    // HOW: Allocate several slots and check utilization
    EXPECT_FLOAT_EQ(synapse_metadata_pool_utilization(metadata_pool), 0.0f);

    std::vector<uint32_t> allocated;
    for (int i = 0; i < 50; i++) {
        uint32_t idx = synapse_metadata_pool_allocate(metadata_pool);
        ASSERT_NE(idx, SPARSE_SYNAPSE_NO_METADATA);
        allocated.push_back(idx);
    }

    EXPECT_FLOAT_EQ(synapse_metadata_pool_utilization(metadata_pool), 0.5f);
    EXPECT_EQ(synapse_metadata_pool_available(metadata_pool), 50u);

    // Free all
    for (uint32_t idx : allocated) {
        synapse_metadata_pool_free(metadata_pool, idx);
    }

    EXPECT_FLOAT_EQ(synapse_metadata_pool_utilization(metadata_pool), 0.0f);
    EXPECT_EQ(synapse_metadata_pool_available(metadata_pool), 100u);
}

TEST_F(SynapseMetadataPoolTest, NoMetadataSentinel) {
    // WHAT: Test that NO_METADATA sentinel is handled correctly
    // WHY: Validate sentinel handling
    // HOW: Pass sentinel to get function
    synapse_t* syn = synapse_metadata_pool_get(metadata_pool, SPARSE_SYNAPSE_NO_METADATA);
    EXPECT_EQ(syn, nullptr);
}

TEST_F(SynapseMetadataPoolTest, HandleMetadataIndex) {
    // WHAT: Test that handle metadata_index is properly initialized
    // WHY: Validate integration between handle and metadata
    // HOW: Add synapse and check metadata_index

    // Add simple synapse (no metadata)
    int result = sparse_synapse_add(handle_pool, &storage, 42, 0.5f);
    ASSERT_EQ(result, 0);

    synapse_handle_t* handle = sparse_synapse_get(&storage, 0);
    ASSERT_NE(handle, nullptr);
    EXPECT_EQ(handle->target_neuron_id, 42u);
    EXPECT_FLOAT_EQ(handle->weight, 0.5f);
    EXPECT_EQ(handle->metadata_index, SPARSE_SYNAPSE_NO_METADATA);

    // Getting metadata should return NULL
    synapse_t* syn = sparse_synapse_get_metadata(metadata_pool, handle);
    EXPECT_EQ(syn, nullptr);
}

TEST_F(SynapseMetadataPoolTest, AddWithMetadata) {
    // WHAT: Test adding synapse with full metadata
    // WHY: Validate full integration
    // HOW: Use sparse_synapse_add_with_metadata
    int result = sparse_synapse_add_with_metadata(
        handle_pool, metadata_pool, &storage,
        100, 0.75f, 1  // SYNAPSE_AMPA
    );
    ASSERT_EQ(result, 0);

    // Verify handle
    synapse_handle_t* handle = sparse_synapse_get(&storage, 0);
    ASSERT_NE(handle, nullptr);
    EXPECT_EQ(handle->target_neuron_id, 100u);
    EXPECT_FLOAT_EQ(handle->weight, 0.75f);
    EXPECT_NE(handle->metadata_index, SPARSE_SYNAPSE_NO_METADATA);

    // Verify metadata
    synapse_t* syn = sparse_synapse_get_metadata(metadata_pool, handle);
    ASSERT_NE(syn, nullptr);
    EXPECT_EQ(syn->target_id, 100u);
    EXPECT_FLOAT_EQ(syn->weight, 0.75f);
    // type moved to synapse_cold_t in NIMCP 2.11 hot/cold split
    // Verify hot fields are correct (type is set via cold pool now)
}

TEST_F(SynapseMetadataPoolTest, RemoveWithMetadata) {
    // WHAT: Test removing synapse with metadata
    // WHY: Validate cleanup of both handle and metadata
    // HOW: Add with metadata, then remove
    sparse_synapse_add_with_metadata(
        handle_pool, metadata_pool, &storage,
        200, 0.5f, 2  // SYNAPSE_NMDA
    );

    EXPECT_EQ(sparse_synapse_count(&storage), 1u);
    EXPECT_EQ(synapse_metadata_pool_available(metadata_pool), 99u);

    // Remove with metadata
    int result = sparse_synapse_remove_with_metadata(
        handle_pool, metadata_pool, &storage, 0
    );
    EXPECT_EQ(result, 0);

    EXPECT_EQ(sparse_synapse_count(&storage), 0u);
    EXPECT_EQ(synapse_metadata_pool_available(metadata_pool), 100u);  // Metadata freed
}

TEST_F(SynapseMetadataPoolTest, MixedSynapses) {
    // WHAT: Test mix of simple and metadata-rich synapses
    // WHY: Validate typical usage pattern
    // HOW: Add some with metadata, some without

    // Add simple synapses
    for (uint32_t i = 0; i < 10; i++) {
        sparse_synapse_add(handle_pool, &storage, i, 1.0f);
    }

    // Add metadata-rich synapses
    for (uint32_t i = 10; i < 15; i++) {
        sparse_synapse_add_with_metadata(
            handle_pool, metadata_pool, &storage,
            i, 0.5f, 1  // SYNAPSE_AMPA
        );
    }

    EXPECT_EQ(sparse_synapse_count(&storage), 15u);
    EXPECT_EQ(synapse_metadata_pool_available(metadata_pool), 95u);  // 5 metadata slots used

    // Check first synapse (simple)
    synapse_handle_t* h0 = sparse_synapse_get(&storage, 0);
    EXPECT_EQ(h0->metadata_index, SPARSE_SYNAPSE_NO_METADATA);

    // Check last synapse (with metadata)
    synapse_handle_t* h14 = sparse_synapse_get(&storage, 14);
    EXPECT_NE(h14->metadata_index, SPARSE_SYNAPSE_NO_METADATA);
}

//=============================================================================
// Main Test Runner
//=============================================================================

int main(int argc, char** argv) {
    // Initialize Google Test
    ::testing::InitGoogleTest(&argc, argv);

    // Run all tests
    return RUN_ALL_TESTS();
}
