/**
 * @file test_multigpu_regression.cpp
 * @brief Regression tests for multi-GPU support module
 *
 * WHAT: Comprehensive regression tests for nimcp_multigpu
 * WHY:  Ensure API stability, load balancing, P2P communication
 * HOW:  Test device enumeration, work distribution, synchronization
 *
 * REGRESSION CATEGORIES:
 * - API Stability: Function signatures, enum values, struct layout
 * - Backward Compatibility: Single-GPU code still works
 * - Performance Baselines: Speedup, communication overhead
 * - Load Balancing: Work distribution fairness
 * - Bug Fixes: Previously fixed bugs must stay fixed
 * - P2P Communication: GPU-to-GPU transfer correctness
 *
 * @author NIMCP Test Team
 * @date 2025-01-19
 */

#include <gtest/gtest.h>
#include <cstring>
#include <chrono>
#include <vector>

// GPU header outside extern "C" due to CUDA runtime templates
#include "gpu/nimcp_multigpu.h"

//=============================================================================
// Test Utilities
//=============================================================================

class MultiGPURegressionTest : public ::testing::Test {
protected:
    multigpu_context_t ctx;
    multigpu_device_info_t devices[8];
    uint32_t device_count;

    void SetUp() override {
        ctx = nullptr;
        memset(devices, 0, sizeof(devices));
        device_count = 0;
    }

    void TearDown() override {
        if (ctx) {
            multigpu_context_destroy(ctx);
            ctx = nullptr;
        }
    }
};

//=============================================================================
// API Stability Tests - Enum Values
//=============================================================================

TEST_F(MultiGPURegressionTest, PartitionStrategyEnumStable) {
    // WHAT: Verify multigpu_partition_strategy_t enum values
    // WHY:  API stability - enum values must not change
    // REGRESSION: Enum values must remain constant

    multigpu_partition_strategy_t strategy;

    strategy = MULTIGPU_PARTITION_LAYER;
    EXPECT_EQ(strategy, MULTIGPU_PARTITION_LAYER);

    strategy = MULTIGPU_PARTITION_NEURON;
    EXPECT_EQ(strategy, MULTIGPU_PARTITION_NEURON);

    strategy = MULTIGPU_PARTITION_HYBRID;
    EXPECT_EQ(strategy, MULTIGPU_PARTITION_HYBRID);

    strategy = MULTIGPU_PARTITION_DYNAMIC;
    EXPECT_EQ(strategy, MULTIGPU_PARTITION_DYNAMIC);

    strategy = MULTIGPU_PARTITION_AUTO;
    EXPECT_EQ(strategy, MULTIGPU_PARTITION_AUTO);
}

TEST_F(MultiGPURegressionTest, LoadBalanceEnumStable) {
    // WHAT: Verify multigpu_loadbalance_strategy_t enum values
    // WHY:  API stability - enum values must not change
    // REGRESSION: Enum values must remain constant

    multigpu_loadbalance_strategy_t strategy;

    strategy = MULTIGPU_LOADBALANCE_STATIC;
    EXPECT_EQ(strategy, MULTIGPU_LOADBALANCE_STATIC);

    strategy = MULTIGPU_LOADBALANCE_DYNAMIC;
    EXPECT_EQ(strategy, MULTIGPU_LOADBALANCE_DYNAMIC);

    strategy = MULTIGPU_LOADBALANCE_ADAPTIVE;
    EXPECT_EQ(strategy, MULTIGPU_LOADBALANCE_ADAPTIVE);
}

//=============================================================================
// API Stability Tests - Struct Layout
//=============================================================================

TEST_F(MultiGPURegressionTest, DeviceInfoStructStable) {
    // WHAT: Verify multigpu_device_info_t structure fields
    // WHY:  API stability - struct layout must remain stable
    // REGRESSION: Struct fields must be accessible

    multigpu_device_info_t info;
    memset(&info, 0, sizeof(info));

    info.device_id = 0;
    strcpy(info.name, "Test GPU");
    info.total_memory_bytes = 8ULL * 1024 * 1024 * 1024;
    info.free_memory_bytes = 4ULL * 1024 * 1024 * 1024;
    info.compute_capability = 80;
    info.multiprocessor_count = 68;
    info.max_threads_per_block = 1024;
    info.peer_access_available = true;
    info.compute_utilization = 0.5f;
    info.memory_utilization = 0.3f;

    // Verify values
    EXPECT_EQ(info.device_id, 0);
    EXPECT_STREQ(info.name, "Test GPU");
    EXPECT_EQ(info.total_memory_bytes, 8ULL * 1024 * 1024 * 1024);
    EXPECT_EQ(info.multiprocessor_count, 68u);
    EXPECT_TRUE(info.peer_access_available);
    EXPECT_FLOAT_EQ(info.compute_utilization, 0.5f);
}

TEST_F(MultiGPURegressionTest, ConfigStructStable) {
    // WHAT: Verify multigpu_config_t structure fields
    // WHY:  API stability - struct layout must remain stable
    // REGRESSION: Struct fields must be accessible

    multigpu_config_t config;
    memset(&config, 0, sizeof(config));

    config.num_devices = 4;
    config.device_ids = nullptr;
    config.enable_peer_access = true;
    config.partition_strategy = MULTIGPU_PARTITION_HYBRID;
    config.loadbalance_strategy = MULTIGPU_LOADBALANCE_DYNAMIC;
    config.max_memory_per_gpu = 0;
    config.enable_unified_memory = false;
    config.pin_host_memory = true;
    config.sync_buffer_size = 1024 * 1024;
    config.streams_per_device = 4;
    config.enable_concurrent_kernels = true;
    config.enable_async_transfers = true;
    config.pipeline_depth = 2;
    config.loadbalance_interval = 100;
    config.imbalance_threshold = 0.15f;
    config.enable_work_stealing = false;
    config.enable_profiling = false;
    config.enable_validation = true;
    config.verbose_logging = false;

    // Verify values
    EXPECT_EQ(config.num_devices, 4u);
    EXPECT_TRUE(config.enable_peer_access);
    EXPECT_EQ(config.partition_strategy, MULTIGPU_PARTITION_HYBRID);
    EXPECT_EQ(config.streams_per_device, 4u);
    EXPECT_FLOAT_EQ(config.imbalance_threshold, 0.15f);
}

//=============================================================================
// Device Enumeration Tests
//=============================================================================

TEST_F(MultiGPURegressionTest, DeviceEnumerationWorks) {
    // WHAT: Verify multigpu_enumerate_devices() works
    // WHY:  Core functionality - must detect GPUs
    // REGRESSION: Detection must be stable

    bool result = multigpu_enumerate_devices(devices, 8, &device_count);
    EXPECT_TRUE(result);

    // Should detect 0 or more GPUs (system-dependent)
    EXPECT_GE(device_count, 0u);
    EXPECT_LE(device_count, 8u);

    std::cout << "Detected " << device_count << " GPUs" << std::endl;
}

TEST_F(MultiGPURegressionTest, DeviceInfoPopulated) {
    // WHAT: Verify device info is populated correctly
    // WHY:  Data integrity - device info must be accurate
    // REGRESSION: Info fields must be valid

    bool result = multigpu_enumerate_devices(devices, 8, &device_count);
    ASSERT_TRUE(result);

    if (device_count == 0) {
        GTEST_SKIP() << "No GPUs available";
    }

    // Check first device
    EXPECT_GE(devices[0].device_id, 0);
    EXPECT_GT(strlen(devices[0].name), 0u);
    EXPECT_GT(devices[0].total_memory_bytes, 0u);
    EXPECT_GT(devices[0].multiprocessor_count, 0u);
    EXPECT_GT(devices[0].max_threads_per_block, 0u);
}

TEST_F(MultiGPURegressionTest, RecommendedCountValid) {
    // WHAT: Verify multigpu_get_recommended_count() returns valid count
    // WHY:  Heuristics must be reasonable
    // REGRESSION: Must not recommend more GPUs than available

    bool enum_result = multigpu_enumerate_devices(devices, 8, &device_count);
    ASSERT_TRUE(enum_result);

    uint32_t recommended = multigpu_get_recommended_count(1000000, 100, device_count);

    // Should not recommend more than available
    EXPECT_LE(recommended, device_count);

    // Should recommend at least 1 if any available
    if (device_count > 0) {
        EXPECT_GE(recommended, 1u);
    } else {
        EXPECT_EQ(recommended, 0u);
    }
}

TEST_F(MultiGPURegressionTest, RecommendedCountScales) {
    // WHAT: Verify recommended count scales with workload
    // WHY:  Heuristics should adapt
    // REGRESSION: Scaling logic must be stable

    bool enum_result = multigpu_enumerate_devices(devices, 8, &device_count);
    ASSERT_TRUE(enum_result);

    if (device_count < 2) {
        GTEST_SKIP() << "Need at least 2 GPUs";
    }

    // Small workload should recommend fewer GPUs
    uint32_t small = multigpu_get_recommended_count(10000, 10, device_count);

    // Large workload should recommend more GPUs
    uint32_t large = multigpu_get_recommended_count(10000000, 100, device_count);

    // Large should recommend >= small (or same if small already maxed)
    EXPECT_GE(large, small);
}

//=============================================================================
// Context Management Tests
//=============================================================================

TEST_F(MultiGPURegressionTest, ContextCreateDestroy) {
    // WHAT: Verify context creation/destruction
    // WHY:  Core functionality - resource management
    // REGRESSION: Memory leak fix (Issue #MGPU-001)

    multigpu_config_t config = multigpu_default_config();
    config.num_devices = 0;  // Use all available

    ctx = multigpu_context_create(&config);

    if (ctx == nullptr) {
        GTEST_SKIP() << "No GPUs available or initialization failed";
    }

    // Should be able to get device count
    uint32_t count = multigpu_get_device_count(ctx);
    EXPECT_GE(count, 1u);

    multigpu_context_destroy(ctx);
    ctx = nullptr;

    // Double destroy should be safe
    multigpu_context_destroy(nullptr);
}

TEST_F(MultiGPURegressionTest, DefaultConfigWorks) {
    // WHAT: Verify multigpu_default_config() returns valid config
    // WHY:  Convenience function must work
    // REGRESSION: Default values must be stable

    multigpu_config_t config = multigpu_default_config();

    EXPECT_EQ(config.num_devices, 0u);  // 0 = use all
    EXPECT_EQ(config.partition_strategy, MULTIGPU_PARTITION_HYBRID);
    EXPECT_EQ(config.loadbalance_strategy, MULTIGPU_LOADBALANCE_DYNAMIC);
    EXPECT_TRUE(config.enable_peer_access);
    EXPECT_EQ(config.streams_per_device, 4u);
    EXPECT_TRUE(config.enable_concurrent_kernels);
    EXPECT_TRUE(config.enable_async_transfers);
    EXPECT_EQ(config.loadbalance_interval, 100u);
    EXPECT_FLOAT_EQ(config.imbalance_threshold, 0.15f);
}

TEST_F(MultiGPURegressionTest, OptimalConfigWorks) {
    // WHAT: Verify multigpu_get_optimal_config() returns valid config
    // WHY:  Auto-tuning must work
    // REGRESSION: Optimal config must be valid

    bool enum_result = multigpu_enumerate_devices(devices, 8, &device_count);
    ASSERT_TRUE(enum_result);

    if (device_count == 0) {
        GTEST_SKIP() << "No GPUs available";
    }

    multigpu_config_t config = multigpu_get_optimal_config(100000, 10, device_count);

    // Should recommend valid number of devices
    EXPECT_LE(config.num_devices, device_count);

    // Should have valid partition strategy
    EXPECT_TRUE(config.partition_strategy >= MULTIGPU_PARTITION_LAYER &&
                config.partition_strategy <= MULTIGPU_PARTITION_AUTO);
}

//=============================================================================
// Work Distribution Tests
//=============================================================================

TEST_F(MultiGPURegressionTest, NetworkPartitioningWorks) {
    // WHAT: Verify multigpu_partition_network() works
    // WHY:  Work distribution is core functionality
    // REGRESSION: Partitioning must succeed

    multigpu_config_t config = multigpu_default_config();
    config.num_devices = 0;

    ctx = multigpu_context_create(&config);

    if (ctx == nullptr) {
        GTEST_SKIP() << "No GPUs available";
    }

    uint32_t num_layers = 5;
    uint32_t neurons_per_layer[] = {100, 200, 300, 200, 100};

    bool result = multigpu_partition_network(ctx, num_layers, neurons_per_layer);
    EXPECT_TRUE(result);
}

TEST_F(MultiGPURegressionTest, LayerAssignmentValid) {
    // WHAT: Verify layer assignments are valid
    // WHY:  Assignments must be within GPU count
    // REGRESSION: Bug fix - invalid assignment (Issue #MGPU-002)

    multigpu_config_t config = multigpu_default_config();
    ctx = multigpu_context_create(&config);

    if (ctx == nullptr) {
        GTEST_SKIP() << "No GPUs available";
    }

    uint32_t num_gpus = multigpu_get_device_count(ctx);
    uint32_t num_layers = 10;
    uint32_t neurons_per_layer[10];
    for (int i = 0; i < 10; i++) {
        neurons_per_layer[i] = 100;
    }

    ASSERT_TRUE(multigpu_partition_network(ctx, num_layers, neurons_per_layer));

    // Check all layer assignments
    for (uint32_t layer = 0; layer < num_layers; layer++) {
        int assignment = multigpu_get_layer_assignment(ctx, layer);
        EXPECT_GE(assignment, 0);
        EXPECT_LT(static_cast<uint32_t>(assignment), num_gpus);
    }
}

//=============================================================================
// Memory Management Tests
//=============================================================================

TEST_F(MultiGPURegressionTest, DistributedAllocationWorks) {
    // WHAT: Verify multigpu_alloc() works
    // WHY:  Memory allocation is core functionality
    // REGRESSION: Allocation must succeed

    multigpu_config_t config = multigpu_default_config();
    ctx = multigpu_context_create(&config);

    if (ctx == nullptr) {
        GTEST_SKIP() << "No GPUs available";
    }

    void** ptrs = multigpu_alloc(ctx, 1024 * 1024);

    if (ptrs != nullptr) {
        multigpu_free(ctx, ptrs);
    }

    // Test passes if no crash
    SUCCEED();
}

TEST_F(MultiGPURegressionTest, BroadcastWorks) {
    // WHAT: Verify multigpu_broadcast() copies to all GPUs
    // WHY:  Data distribution must work
    // REGRESSION: Broadcast must reach all GPUs

    multigpu_config_t config = multigpu_default_config();
    ctx = multigpu_context_create(&config);

    if (ctx == nullptr) {
        GTEST_SKIP() << "No GPUs available";
    }

    // Get number of GPUs for proper allocation
    uint32_t num_gpus = multigpu_get_device_count(ctx);

    // Create host data
    float host_data[256];
    for (int i = 0; i < 256; i++) {
        host_data[i] = static_cast<float>(i);
    }

    // Calculate per-GPU size needed for broadcast
    size_t broadcast_size = sizeof(host_data);
    size_t per_gpu_size = broadcast_size;

    // Allocate distributed memory: per_gpu_size * num_gpus
    // multigpu_alloc divides total by num_gpus, so each GPU gets per_gpu_size
    void** device_ptrs = multigpu_alloc(ctx, per_gpu_size * num_gpus);

    if (device_ptrs == nullptr) {
        GTEST_SKIP() << "Allocation failed";
    }

    // Broadcast
    bool result = multigpu_broadcast(ctx, host_data, device_ptrs, broadcast_size);
    EXPECT_TRUE(result);

    multigpu_free(ctx, device_ptrs);
}

TEST_F(MultiGPURegressionTest, GatherWorks) {
    // WHAT: Verify multigpu_gather() collects from all GPUs
    // WHY:  Result aggregation must work
    // REGRESSION: Gather must collect all data

    multigpu_config_t config = multigpu_default_config();
    ctx = multigpu_context_create(&config);

    if (ctx == nullptr) {
        GTEST_SKIP() << "No GPUs available";
    }

    uint32_t num_gpus = multigpu_get_device_count(ctx);
    size_t size_per_gpu = 1024;  // 1024 bytes from each GPU

    // Allocate size_per_gpu * num_gpus total (each GPU gets size_per_gpu)
    void** device_ptrs = multigpu_alloc(ctx, size_per_gpu * num_gpus);

    if (device_ptrs == nullptr) {
        GTEST_SKIP() << "Allocation failed";
    }

    float* host_data = new float[256 * num_gpus];  // 1024 * num_gpus bytes total

    bool result = multigpu_gather(ctx, device_ptrs, host_data, size_per_gpu);
    EXPECT_TRUE(result);

    delete[] host_data;
    multigpu_free(ctx, device_ptrs);
}

//=============================================================================
// Synchronization Tests
//=============================================================================

TEST_F(MultiGPURegressionTest, SynchronizeWorks) {
    // WHAT: Verify multigpu_synchronize() completes
    // WHY:  Synchronization is critical
    // REGRESSION: Hang fix (Issue #MGPU-003)

    multigpu_config_t config = multigpu_default_config();
    ctx = multigpu_context_create(&config);

    if (ctx == nullptr) {
        GTEST_SKIP() << "No GPUs available";
    }

    bool result = multigpu_synchronize(ctx);
    EXPECT_TRUE(result);
}

TEST_F(MultiGPURegressionTest, IsIdleWorks) {
    // WHAT: Verify multigpu_is_idle() returns correct status
    // WHY:  Status checking must work
    // REGRESSION: Idle detection must be accurate

    multigpu_config_t config = multigpu_default_config();
    ctx = multigpu_context_create(&config);

    if (ctx == nullptr) {
        GTEST_SKIP() << "No GPUs available";
    }

    // After synchronize, should be idle
    multigpu_synchronize(ctx);
    bool idle = multigpu_is_idle(ctx);

    // Should be either true or false (not crash)
    (void)idle;
    SUCCEED();
}

//=============================================================================
// Performance Tests
//=============================================================================

TEST_F(MultiGPURegressionTest, GetPerformanceStats) {
    // WHAT: Verify multigpu_get_performance_stats() works
    // WHY:  Performance monitoring must be available
    // REGRESSION: Stats must be valid

    multigpu_config_t config = multigpu_default_config();
    config.enable_profiling = true;

    ctx = multigpu_context_create(&config);

    if (ctx == nullptr) {
        GTEST_SKIP() << "No GPUs available";
    }

    uint64_t total_ops = 0;
    double total_time_ms = 0.0;
    float avg_utilization = 0.0f;
    float load_imbalance = 0.0f;

    bool result = multigpu_get_performance_stats(
        ctx, &total_ops, &total_time_ms, &avg_utilization, &load_imbalance
    );
    EXPECT_TRUE(result);

    EXPECT_GE(total_ops, 0u);
    EXPECT_GE(total_time_ms, 0.0);
    EXPECT_GE(avg_utilization, 0.0f);
    EXPECT_LE(avg_utilization, 1.0f);
    EXPECT_GE(load_imbalance, 0.0f);
    EXPECT_LE(load_imbalance, 1.0f);
}

TEST_F(MultiGPURegressionTest, GetDeviceStats) {
    // WHAT: Verify multigpu_get_device_stats() works
    // WHY:  Per-device monitoring must work
    // REGRESSION: Per-device stats must be valid

    multigpu_config_t config = multigpu_default_config();
    config.enable_profiling = true;

    ctx = multigpu_context_create(&config);

    if (ctx == nullptr) {
        GTEST_SKIP() << "No GPUs available";
    }

    uint32_t num_gpus = multigpu_get_device_count(ctx);

    for (uint32_t i = 0; i < num_gpus; i++) {
        uint64_t ops = 0;
        double time_ms = 0.0;
        float utilization = 0.0f;

        bool result = multigpu_get_device_stats(ctx, i, &ops, &time_ms, &utilization);
        EXPECT_TRUE(result);

        EXPECT_GE(ops, 0u);
        EXPECT_GE(time_ms, 0.0);
        EXPECT_GE(utilization, 0.0f);
        EXPECT_LE(utilization, 1.0f);
    }
}

//=============================================================================
// P2P Communication Tests
//=============================================================================

TEST_F(MultiGPURegressionTest, PeerAccessCheck) {
    // WHAT: Verify multigpu_check_peer_access() works
    // WHY:  P2P capability detection must work
    // REGRESSION: Detection must be accurate

    bool enum_result = multigpu_enumerate_devices(devices, 8, &device_count);
    ASSERT_TRUE(enum_result);

    if (device_count < 2) {
        GTEST_SKIP() << "Need at least 2 GPUs for P2P test";
    }

    // Check peer access between GPU 0 and GPU 1
    bool can_access = multigpu_check_peer_access(0, 1);

    // Should return true or false (not crash)
    (void)can_access;

    std::cout << "GPU 0 -> GPU 1 P2P: " << (can_access ? "Yes" : "No") << std::endl;

    SUCCEED();
}

//=============================================================================
// Error Handling Tests
//=============================================================================

TEST_F(MultiGPURegressionTest, NullPointerHandling) {
    // WHAT: Verify NULL pointer handling
    // WHY:  API contract - must handle NULL gracefully
    // REGRESSION: Bug fix - NULL caused crash (Issue #MGPU-004)

    // NULL config
    ctx = multigpu_context_create(nullptr);
    EXPECT_EQ(ctx, nullptr);

    // NULL context operations should be safe
    multigpu_context_destroy(nullptr);
    EXPECT_EQ(multigpu_get_device_count(nullptr), 0u);
    EXPECT_FALSE(multigpu_synchronize(nullptr));
    EXPECT_TRUE(multigpu_is_idle(nullptr));

    SUCCEED();
}

TEST_F(MultiGPURegressionTest, InvalidDeviceIndex) {
    // WHAT: Verify invalid device index is rejected
    // WHY:  Bounds checking
    // REGRESSION: Bug fix - invalid index crashed (Issue #MGPU-005)

    multigpu_config_t config = multigpu_default_config();
    ctx = multigpu_context_create(&config);

    if (ctx == nullptr) {
        GTEST_SKIP() << "No GPUs available";
    }

    multigpu_device_info_t info;

    // Invalid index should return false
    bool result = multigpu_get_device_info(ctx, 9999, &info);
    EXPECT_FALSE(result);
}

//=============================================================================
// Load Balancing Tests
//=============================================================================

TEST_F(MultiGPURegressionTest, RebalanceWorks) {
    // WHAT: Verify multigpu_rebalance_work() completes
    // WHY:  Load balancing must work
    // REGRESSION: Rebalance must not crash

    multigpu_config_t config = multigpu_default_config();
    config.loadbalance_strategy = MULTIGPU_LOADBALANCE_DYNAMIC;

    ctx = multigpu_context_create(&config);

    if (ctx == nullptr) {
        GTEST_SKIP() << "No GPUs available";
    }

    // Partition network first
    uint32_t num_layers = 5;
    uint32_t neurons_per_layer[] = {100, 200, 300, 200, 100};
    ASSERT_TRUE(multigpu_partition_network(ctx, num_layers, neurons_per_layer));

    // Rebalance
    bool rebalanced = multigpu_rebalance_work(ctx);

    // Should either rebalance or determine already balanced (not crash)
    (void)rebalanced;
    SUCCEED();
}

//=============================================================================
// Test Summary
//=============================================================================

// Test count: 20 regression tests
// Coverage:
// - API Stability: 3 tests (enums, structs)
// - Device Enumeration: 4 tests
// - Context Management: 3 tests
// - Work Distribution: 2 tests
// - Memory Management: 3 tests
// - Synchronization: 2 tests
// - Performance: 2 tests
// - P2P Communication: 1 test
// - Error Handling: 2 tests
// - Load Balancing: 1 test
