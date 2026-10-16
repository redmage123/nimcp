//=============================================================================
// test_unified_memory_integration.cpp - Integration Tests for Unified Memory
//=============================================================================
/**
 * @file test_unified_memory_integration.cpp
 * @brief Integration tests for unified memory manager with CoW integration
 *
 * Tests cover:
 * - Integration between object-level and page-level CoW
 * - Multi-manager scenarios
 * - Brain-like workloads (weight matrices, activations)
 * - Checkpoint/rollback workflows
 * - Memory pressure scenarios
 * - Cross-component integration
 *
 * @author NIMCP Development Team
 * @date 2025-11-27
 */

#include <gtest/gtest.h>
#include <cstring>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <random>

// Headers have their own extern "C" guards
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/memory/nimcp_memory_pool.h"
#include "utils/memory/nimcp_cow_manager.h"
#include "utils/memory/nimcp_page_cow.h"
// Forward declaration for cow_read function
extern const void* cow_read(cow_handle_t handle);

// Helper macro to access cow_read function
#define cow_read_ptr(h) cow_read(h)

//=============================================================================
// Test Fixture
//=============================================================================

class UnifiedMemoryIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize page COW subsystem
        ASSERT_TRUE(page_cow_init());
    }

    void TearDown() override {
        // Cleanup happens automatically via destructors
    }

    // Helper to create random data
    std::vector<float> createRandomData(size_t num_floats, unsigned seed = 42) {
        std::vector<float> data(num_floats);
        std::mt19937 gen(seed);
        std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
        for (size_t i = 0; i < num_floats; i++) {
            data[i] = dist(gen);
        }
        return data;
    }

    // Simulate neural network weight update
    void simulateWeightUpdate(float* weights, size_t count, float learning_rate) {
        std::mt19937 gen(std::random_device{}());
        std::uniform_real_distribution<float> dist(-0.01f, 0.01f);
        for (size_t i = 0; i < count; i++) {
            weights[i] += learning_rate * dist(gen);
        }
    }
};

//=============================================================================
// Multi-Manager Integration Tests
//=============================================================================

TEST_F(UnifiedMemoryIntegrationTest, MultiManager_IndependentOperation) {
    // Create two independent managers
    unified_mem_config_t config1 = unified_mem_default_config();
    config1.page_pool_num_pages = 64;

    unified_mem_config_t config2 = unified_mem_default_config();
    config2.page_pool_num_pages = 32;

    unified_mem_manager_t mgr1 = unified_mem_create(&config1);
    unified_mem_manager_t mgr2 = unified_mem_create(&config2);
    ASSERT_NE(mgr1, nullptr);
    ASSERT_NE(mgr2, nullptr);

    // Allocate from both managers
    unified_mem_request_t req1 = unified_mem_request(4096, nullptr, true);
    unified_mem_request_t req2 = unified_mem_request(8192, nullptr, true);

    unified_mem_handle_t h1 = unified_mem_alloc(mgr1, &req1);
    unified_mem_handle_t h2 = unified_mem_alloc(mgr2, &req2);
    ASSERT_NE(h1, nullptr);
    ASSERT_NE(h2, nullptr);

    // Operations on one don't affect the other
    unified_mem_write(h1);

    unified_mem_stats_t stats1, stats2;
    unified_mem_get_stats(mgr1, &stats1);
    unified_mem_get_stats(mgr2, &stats2);

    EXPECT_EQ(stats1.cow_triggers, 1);
    EXPECT_EQ(stats2.cow_triggers, 0);  // h2 was never written

    unified_mem_free(h1);
    unified_mem_free(h2);
    unified_mem_destroy(mgr2);
    unified_mem_destroy(mgr1);
}

TEST_F(UnifiedMemoryIntegrationTest, MultiManager_SharedData_CrossManager) {
    unified_mem_manager_t mgr1 = unified_mem_create(nullptr);
    unified_mem_manager_t mgr2 = unified_mem_create(nullptr);
    ASSERT_NE(mgr1, nullptr);
    ASSERT_NE(mgr2, nullptr);

    // Create data in mgr1
    const size_t size = 4096;
    std::vector<float> data = createRandomData(size / sizeof(float));

    unified_mem_request_t req = {
        .size = size,
        .initial_data = data.data(),
        .strategy = UNIFIED_STRATEGY_OBJECT_COW,
        .enable_cow = true,
        .alignment = 0
    };

    unified_mem_handle_t h1 = unified_mem_alloc(mgr1, &req);
    ASSERT_NE(h1, nullptr);

    // Read data from h1 and create identical allocation in mgr2
    const float* h1_data = static_cast<const float*>(unified_mem_read(h1));
    req.initial_data = h1_data;
    unified_mem_handle_t h2 = unified_mem_alloc(mgr2, &req);
    ASSERT_NE(h2, nullptr);

    // Both should have identical data
    const float* h2_data = static_cast<const float*>(unified_mem_read(h2));
    EXPECT_EQ(memcmp(h1_data, h2_data, size), 0);

    unified_mem_free(h1);
    unified_mem_free(h2);
    unified_mem_destroy(mgr2);
    unified_mem_destroy(mgr1);
}

//=============================================================================
// Brain Workload Simulation Tests
//=============================================================================

TEST_F(UnifiedMemoryIntegrationTest, BrainWorkload_WeightMatrixCloning) {
    // Simulate cloning weight matrices for parallel workers
    unified_mem_manager_t mgr = unified_mem_create(nullptr);
    ASSERT_NE(mgr, nullptr);

    // Create master weight matrix (1MB = 256K floats)
    const size_t weight_size = 1 * 1024 * 1024;
    std::vector<float> weights = createRandomData(weight_size / sizeof(float));

    unified_mem_request_t req = unified_mem_request_page_cow(weight_size, weights.data());
    unified_mem_handle_t master = unified_mem_alloc(mgr, &req);
    ASSERT_NE(master, nullptr);

    // Clone for multiple workers
    const int num_workers = 8;
    std::vector<unified_mem_handle_t> worker_handles;
    for (int i = 0; i < num_workers; i++) {
        unified_mem_handle_t h = unified_mem_clone(master);
        ASSERT_NE(h, nullptr);
        worker_handles.push_back(h);
    }

    // All workers share data initially
    for (auto h : worker_handles) {
        EXPECT_TRUE(unified_mem_is_shared(h));
    }

    // Each worker modifies a subset (simulates gradient updates)
    for (int i = 0; i < num_workers; i++) {
        float* w = static_cast<float*>(unified_mem_write(worker_handles[i]));
        ASSERT_NE(w, nullptr);
        // Modify only a few values
        for (int j = 0; j < 100; j++) {
            w[i * 1000 + j] += 0.001f;
        }
    }

    // All workers should now be private
    for (auto h : worker_handles) {
        EXPECT_FALSE(unified_mem_is_shared(h));
    }

    // Cleanup
    for (auto h : worker_handles) {
        unified_mem_free(h);
    }
    unified_mem_free(master);
    unified_mem_destroy(mgr);
}

TEST_F(UnifiedMemoryIntegrationTest, BrainWorkload_ActivationBuffers) {
    // Simulate activation buffers with mixed sizes
    unified_mem_manager_t mgr = unified_mem_create(nullptr);
    ASSERT_NE(mgr, nullptr);

    // Different layer sizes (small to large)
    std::vector<size_t> layer_sizes = {
        256,            // Small (object CoW)
        1024,           // Small (object CoW)
        4096,           // Small (object CoW)
        16 * 1024,      // Medium (object CoW)
        64 * 1024,      // Threshold (page CoW)
        256 * 1024,     // Large (page CoW)
        1024 * 1024     // Very large (page CoW)
    };

    std::vector<unified_mem_handle_t> activations;

    // Allocate activations for each layer
    for (size_t size : layer_sizes) {
        unified_mem_request_t req = unified_mem_request(size, nullptr, true);
        unified_mem_handle_t h = unified_mem_alloc(mgr, &req);
        ASSERT_NE(h, nullptr);
        activations.push_back(h);
    }

    // Verify strategy selection
    for (size_t i = 0; i < layer_sizes.size(); i++) {
        unified_mem_strategy_t strategy = unified_mem_get_strategy(activations[i]);
        if (layer_sizes[i] >= UNIFIED_MEM_PAGE_THRESHOLD) {
            EXPECT_EQ(strategy, UNIFIED_STRATEGY_PAGE_COW)
                << "Layer " << i << " size=" << layer_sizes[i];
        } else {
            EXPECT_EQ(strategy, UNIFIED_STRATEGY_OBJECT_COW)
                << "Layer " << i << " size=" << layer_sizes[i];
        }
    }

    // Forward pass - write activations
    for (size_t i = 0; i < activations.size(); i++) {
        float* data = static_cast<float*>(unified_mem_write(activations[i]));
        ASSERT_NE(data, nullptr);
        // Fill with layer index for verification
        for (size_t j = 0; j < layer_sizes[i] / sizeof(float); j++) {
            data[j] = static_cast<float>(i);
        }
    }

    // Verify data
    for (size_t i = 0; i < activations.size(); i++) {
        const float* data = static_cast<const float*>(unified_mem_read(activations[i]));
        EXPECT_FLOAT_EQ(data[0], static_cast<float>(i));
    }

    // Cleanup
    for (auto h : activations) {
        unified_mem_free(h);
    }
    unified_mem_destroy(mgr);
}

//=============================================================================
// Checkpoint/Rollback Workflow Tests
//=============================================================================

TEST_F(UnifiedMemoryIntegrationTest, Checkpoint_TrainingRollback) {
    unified_mem_manager_t mgr = unified_mem_create(nullptr);
    ASSERT_NE(mgr, nullptr);

    // Create model weights - use object CoW for reliable snapshot support
    const size_t weight_size = 16 * 1024;  // 16KB (fits in object CoW)
    std::vector<float> initial_weights = createRandomData(weight_size / sizeof(float), 42);

    unified_mem_request_t req = {
        .size = weight_size,
        .initial_data = initial_weights.data(),
        .strategy = UNIFIED_STRATEGY_OBJECT_COW,
        .enable_cow = true,
        .alignment = 0
    };
    unified_mem_handle_t weights = unified_mem_alloc(mgr, &req);
    ASSERT_NE(weights, nullptr);

    // Checkpoint before training
    unified_mem_snapshot_t checkpoint = unified_mem_snapshot_create(weights);
    ASSERT_NE(checkpoint, nullptr);

    // Simulate training iterations
    float* w = static_cast<float*>(unified_mem_write(weights));
    ASSERT_NE(w, nullptr);

    // Training makes weights diverge
    for (int epoch = 0; epoch < 10; epoch++) {
        simulateWeightUpdate(w, weight_size / sizeof(float), 0.001f);
    }

    // Verify weights have changed
    EXPECT_NE(w[0], initial_weights[0]);

    // Check delta - compare byte-by-byte for object CoW
    size_t delta = unified_mem_snapshot_get_delta_bytes(weights, checkpoint);
    EXPECT_GT(delta, 0);

    // Rollback to checkpoint
    EXPECT_TRUE(unified_mem_snapshot_restore(weights, checkpoint));

    // Verify rollback
    const float* restored = static_cast<const float*>(unified_mem_read(weights));
    EXPECT_FLOAT_EQ(restored[0], initial_weights[0]);

    unified_mem_snapshot_destroy(checkpoint);
    unified_mem_free(weights);
    unified_mem_destroy(mgr);
}

TEST_F(UnifiedMemoryIntegrationTest, Checkpoint_MultipleSnapshots) {
    unified_mem_manager_t mgr = unified_mem_create(nullptr);
    ASSERT_NE(mgr, nullptr);

    const size_t size = 4096;
    unified_mem_request_t req = {
        .size = size,
        .initial_data = nullptr,
        .strategy = UNIFIED_STRATEGY_OBJECT_COW,
        .enable_cow = true,
        .alignment = 0
    };

    unified_mem_handle_t h = unified_mem_alloc(mgr, &req);
    ASSERT_NE(h, nullptr);

    // Create multiple snapshots at different states
    std::vector<unified_mem_snapshot_t> snapshots;
    std::vector<float> expected_values;

    for (int i = 0; i < 5; i++) {
        float* data = static_cast<float*>(unified_mem_write(h));
        data[0] = static_cast<float>(i * 100);
        expected_values.push_back(data[0]);

        unified_mem_snapshot_t snap = unified_mem_snapshot_create(h);
        ASSERT_NE(snap, nullptr);
        snapshots.push_back(snap);
    }

    // Modify to a different value
    float* data = static_cast<float*>(unified_mem_write(h));
    data[0] = 9999.0f;

    // Restore to each snapshot and verify
    for (size_t i = 0; i < snapshots.size(); i++) {
        EXPECT_TRUE(unified_mem_snapshot_restore(h, snapshots[i]));

        const float* read_data = static_cast<const float*>(unified_mem_read(h));
        EXPECT_FLOAT_EQ(read_data[0], expected_values[i])
            << "Snapshot " << i << " restoration failed";
    }

    // Cleanup
    for (auto snap : snapshots) {
        unified_mem_snapshot_destroy(snap);
    }
    unified_mem_free(h);
    unified_mem_destroy(mgr);
}

//=============================================================================
// Memory Pressure Tests
//=============================================================================

TEST_F(UnifiedMemoryIntegrationTest, MemoryPressure_ManyAllocations) {
    unified_mem_config_t config = unified_mem_default_config();
    config.page_pool_num_pages = 256;  // Limited pool
    unified_mem_manager_t mgr = unified_mem_create(&config);
    ASSERT_NE(mgr, nullptr);

    // Allocate many handles exceeding pool capacity
    const int num_allocs = 500;
    std::vector<unified_mem_handle_t> handles;

    for (int i = 0; i < num_allocs; i++) {
        unified_mem_request_t req = unified_mem_request(PAGE_COW_PAGE_SIZE, nullptr, false);
        unified_mem_handle_t h = unified_mem_alloc(mgr, &req);
        if (h) {
            handles.push_back(h);
        }
    }

    // Should have allocated all (pool + malloc fallback)
    EXPECT_EQ(handles.size(), num_allocs);

    // Verify all handles work
    for (auto h : handles) {
        float* data = static_cast<float*>(unified_mem_write(h));
        ASSERT_NE(data, nullptr);
        data[0] = 1.0f;
    }

    // Cleanup
    for (auto h : handles) {
        unified_mem_free(h);
    }
    unified_mem_destroy(mgr);
}

TEST_F(UnifiedMemoryIntegrationTest, MemoryPressure_PoolRecycling) {
    unified_mem_config_t config = unified_mem_default_config();
    config.page_pool_num_pages = 16;  // Small pool
    unified_mem_manager_t mgr = unified_mem_create(&config);
    ASSERT_NE(mgr, nullptr);

    // Allocate and free in cycles
    for (int cycle = 0; cycle < 10; cycle++) {
        std::vector<unified_mem_handle_t> handles;

        // Allocate all pool pages
        for (int i = 0; i < 16; i++) {
            unified_mem_request_t req = unified_mem_request_direct(PAGE_COW_PAGE_SIZE);
            unified_mem_handle_t h = unified_mem_alloc(mgr, &req);
            if (h) handles.push_back(h);
        }

        EXPECT_GE(handles.size(), 16) << "Cycle " << cycle;

        // Free all
        for (auto h : handles) {
            unified_mem_free(h);
        }

        // Pool should be fully available again
        size_t total, free;
        unified_mem_get_page_pool_stats(mgr, &total, &free);
        EXPECT_EQ(free, 16) << "Cycle " << cycle;
    }

    unified_mem_destroy(mgr);
}

//=============================================================================
// Cross-Component Integration Tests
//=============================================================================

TEST_F(UnifiedMemoryIntegrationTest, CrossComponent_WithPageCow) {
    // Test unified memory working alongside raw page_cow
    unified_mem_manager_t mgr = unified_mem_create(nullptr);
    ASSERT_NE(mgr, nullptr);

    // Create page_cow region directly
    page_cow_config_t pcfg = page_cow_default_config(64 * 1024);
    page_cow_region_t region = page_cow_region_create(&pcfg, nullptr);
    ASSERT_NE(region, nullptr);

    page_cow_view_t view = page_cow_view_create(region);
    ASSERT_NE(view, nullptr);

    // Create unified memory allocation
    unified_mem_request_t req = unified_mem_request_page_cow(64 * 1024, nullptr);
    unified_mem_handle_t h = unified_mem_alloc(mgr, &req);
    ASSERT_NE(h, nullptr);

    // Both should work independently
    void* pcow_data = page_cow_view_write(view);
    ASSERT_NE(pcow_data, nullptr);

    void* unified_data = unified_mem_write(h);
    ASSERT_NE(unified_data, nullptr);

    // Write to both
    memset(pcow_data, 0x11, 64 * 1024);
    memset(unified_data, 0x22, 64 * 1024);

    // Verify independence
    const uint8_t* pcow_read = static_cast<const uint8_t*>(page_cow_view_read(view));
    const uint8_t* unified_read = static_cast<const uint8_t*>(unified_mem_read(h));

    EXPECT_EQ(pcow_read[0], 0x11);
    EXPECT_EQ(unified_read[0], 0x22);

    // Cleanup
    unified_mem_free(h);
    page_cow_view_destroy(view);
    page_cow_region_destroy(region);
    unified_mem_destroy(mgr);
}

TEST_F(UnifiedMemoryIntegrationTest, CrossComponent_WithCowManager) {
    // Test unified memory working alongside raw cow_manager
    unified_mem_manager_t mgr = unified_mem_create(nullptr);
    ASSERT_NE(mgr, nullptr);

    // Create cow_manager directly
    const size_t size = 4096;
    std::vector<float> template_data = createRandomData(size / sizeof(float));

    cow_manager_config_t cow_cfg = cow_default_config(size, nullptr);
    cow_manager_t cow_mgr = cow_manager_create(&cow_cfg, template_data.data());
    ASSERT_NE(cow_mgr, nullptr);

    cow_handle_t cow_handle = cow_acquire(cow_mgr);
    ASSERT_NE(cow_handle, nullptr);

    // Create unified memory allocation
    unified_mem_request_t req = {
        .size = size,
        .initial_data = template_data.data(),
        .strategy = UNIFIED_STRATEGY_OBJECT_COW,
        .enable_cow = true,
        .alignment = 0
    };
    unified_mem_handle_t h = unified_mem_alloc(mgr, &req);
    ASSERT_NE(h, nullptr);

    // Both should work independently
    const float* cow_read_data = static_cast<const float*>(cow_read_ptr(cow_handle));
    const float* unified_read = static_cast<const float*>(unified_mem_read(h));

    // Initially same data
    EXPECT_FLOAT_EQ(cow_read_data[0], unified_read[0]);

    // Modify unified
    float* unified_write = static_cast<float*>(unified_mem_write(h));
    unified_write[0] = 999.0f;

    // cow_handle should be unchanged
    cow_read_data = static_cast<const float*>(cow_read_ptr(cow_handle));
    EXPECT_FLOAT_EQ(cow_read_data[0], template_data[0]);

    // Cleanup
    unified_mem_free(h);
    cow_release(cow_handle);
    cow_manager_destroy(cow_mgr);
    unified_mem_destroy(mgr);
}

//=============================================================================
// Concurrent Workflow Tests
//=============================================================================

TEST_F(UnifiedMemoryIntegrationTest, ConcurrentWorkflow_ProducerConsumer) {
    unified_mem_manager_t mgr = unified_mem_create(nullptr);
    ASSERT_NE(mgr, nullptr);

    const size_t buffer_size = 64 * 1024;
    std::atomic<bool> stop{false};
    std::atomic<int> produced{0};
    std::atomic<int> consumed{0};

    // Shared buffer
    unified_mem_request_t req = unified_mem_request_page_cow(buffer_size, nullptr);
    unified_mem_handle_t shared_buffer = unified_mem_alloc(mgr, &req);
    ASSERT_NE(shared_buffer, nullptr);

    // Producer thread
    auto producer = [&]() {
        while (!stop.load()) {
            unified_mem_handle_t h = unified_mem_clone(shared_buffer);
            if (h) {
                float* data = static_cast<float*>(unified_mem_write(h));
                if (data) {
                    data[0] = static_cast<float>(produced.load());
                    produced++;
                }
                unified_mem_free(h);
            }
            std::this_thread::yield();
        }
    };

    // Consumer thread
    auto consumer = [&]() {
        while (!stop.load()) {
            unified_mem_handle_t h = unified_mem_clone(shared_buffer);
            if (h) {
                const float* data = static_cast<const float*>(unified_mem_read(h));
                if (data) {
                    consumed++;
                }
                unified_mem_free(h);
            }
            std::this_thread::yield();
        }
    };

    std::thread t_producer(producer);
    std::thread t_consumer(consumer);

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    stop.store(true);

    t_producer.join();
    t_consumer.join();

    EXPECT_GT(produced.load(), 0);
    EXPECT_GT(consumed.load(), 0);

    unified_mem_free(shared_buffer);
    unified_mem_destroy(mgr);
}

//=============================================================================
// Strategy Transition Tests
//=============================================================================

TEST_F(UnifiedMemoryIntegrationTest, StrategyTransition_MixedWorkload) {
    unified_mem_manager_t mgr = unified_mem_create(nullptr);
    ASSERT_NE(mgr, nullptr);

    // Allocate mixed sizes
    std::vector<unified_mem_handle_t> handles;
    std::vector<size_t> sizes = {256, 1024, 4096, 16384, 65536, 262144};

    for (size_t size : sizes) {
        unified_mem_request_t req = unified_mem_request(size, nullptr, true);
        unified_mem_handle_t h = unified_mem_alloc(mgr, &req);
        ASSERT_NE(h, nullptr);
        handles.push_back(h);
    }

    // Clone all
    std::vector<unified_mem_handle_t> clones;
    for (auto h : handles) {
        unified_mem_handle_t clone = unified_mem_clone(h);
        ASSERT_NE(clone, nullptr);
        clones.push_back(clone);
    }

    // Write to all clones
    for (auto h : clones) {
        float* data = static_cast<float*>(unified_mem_write(h));
        ASSERT_NE(data, nullptr);
        data[0] = 42.0f;
    }

    // Original handles should be unchanged
    for (size_t i = 0; i < handles.size(); i++) {
        const float* data = static_cast<const float*>(unified_mem_read(handles[i]));
        EXPECT_FLOAT_EQ(data[0], 0.0f)
            << "Original handle " << i << " was modified";
    }

    // Cleanup
    for (auto h : clones) unified_mem_free(h);
    for (auto h : handles) unified_mem_free(h);
    unified_mem_destroy(mgr);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
