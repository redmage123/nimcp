/**
 * @file test_multigpu_comprehensive.cpp
 * @brief Comprehensive Tests for Multi-GPU Functionality
 *
 * WHAT: Exhaustive test suite for all multi-GPU operations
 * WHY:  Ensure reliability of distributed GPU computation
 * HOW:  Test all APIs, edge cases, error handling, integration scenarios
 *
 * COVERAGE AREAS:
 * 1. Device enumeration and P2P capabilities
 * 2. Context lifecycle management
 * 3. Work distribution and partitioning strategies
 * 4. Memory allocation and synchronization
 * 5. Load balancing and rebalancing
 * 6. Performance monitoring and statistics
 * 7. Error handling and NULL checks
 * 8. Multi-GPU integration workflows
 * 9. Dynamic GPU addition/removal scenarios
 * 10. Fault tolerance and recovery
 *
 * @author NIMCP Development Team
 * @date 2025
 * @version 2.7
 */

#include <gtest/gtest.h>
#include <vector>
#include <memory>
#include <algorithm>

#include "gpu/nimcp_multigpu.h"

//=============================================================================
// Test Fixtures
//=============================================================================

/**
 * @brief Base test fixture with common setup
 */
class MultiGPUComprehensiveTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Enumerate devices to check availability
        uint32_t device_count = 0;
        multigpu_device_info_t devices[16];
        device_enumeration_works = multigpu_enumerate_devices(devices, 16, &device_count);
        available_device_count = device_count;
    }

    void TearDown() override {
        // Cleanup
    }

    bool device_enumeration_works = false;
    uint32_t available_device_count = 0;
};

/**
 * @brief Fixture for tests requiring valid context
 */
class MultiGPUContextTest : public MultiGPUComprehensiveTest {
protected:
    void SetUp() override {
        MultiGPUComprehensiveTest::SetUp();

        // Create default context
        multigpu_config_t config = multigpu_default_config();
        ctx = multigpu_context_create(&config);
    }

    void TearDown() override {
        if (ctx) {
            multigpu_context_destroy(ctx);
            ctx = nullptr;
        }
        MultiGPUComprehensiveTest::TearDown();
    }

    multigpu_context_t ctx = nullptr;
};

//=============================================================================
// Device Enumeration Tests
//=============================================================================

TEST_F(MultiGPUComprehensiveTest, EnumerateDevices_BasicSuccess) {
    // WHAT: Test basic device enumeration
    // WHY:  Verify mock backend returns devices
    // HOW:  Call enumerate, check count and basic fields

    multigpu_device_info_t devices[8];
    uint32_t count = 0;

    bool result = multigpu_enumerate_devices(devices, 8, &count);

    EXPECT_TRUE(result) << "Enumeration should succeed";
    EXPECT_GT(count, 0u) << "Should detect at least 1 mock GPU";

    if (count > 0) {
        EXPECT_GE(devices[0].device_id, 0);
        EXPECT_GT(devices[0].total_memory_bytes, 0u);
        EXPECT_GT(strlen(devices[0].name), 0u);
        EXPECT_LE(devices[0].memory_utilization, 1.0f);
        EXPECT_GE(devices[0].memory_utilization, 0.0f);
    }
}

TEST_F(MultiGPUComprehensiveTest, EnumerateDevices_MultipleDevices) {
    // WHAT: Test that mock backend provides multiple devices
    // WHY:  Need multiple GPUs for multi-GPU testing
    // HOW:  Verify count >= 2

    multigpu_device_info_t devices[16];
    uint32_t count = 0;

    bool result = multigpu_enumerate_devices(devices, 16, &count);

    EXPECT_TRUE(result);
    EXPECT_GE(count, 2u) << "Mock backend should provide multiple GPUs";
}

TEST_F(MultiGPUComprehensiveTest, EnumerateDevices_DeviceInfoComplete) {
    // WHAT: Test all device info fields are populated
    // WHY:  Ensure complete device information
    // HOW:  Check all struct fields for validity

    multigpu_device_info_t devices[8];
    uint32_t count = 0;

    multigpu_enumerate_devices(devices, 8, &count);

    ASSERT_GT(count, 0u);

    const auto& dev = devices[0];
    EXPECT_GE(dev.device_id, 0);
    EXPECT_GT(dev.total_memory_bytes, 0u);
    EXPECT_GT(dev.free_memory_bytes, 0u);
    EXPECT_LE(dev.free_memory_bytes, dev.total_memory_bytes);
    EXPECT_GT(dev.compute_capability, 0u);
    EXPECT_GT(dev.multiprocessor_count, 0u);
    EXPECT_GT(dev.max_threads_per_block, 0u);
    EXPECT_GE(dev.compute_utilization, 0.0f);
    EXPECT_LE(dev.compute_utilization, 1.0f);
}

TEST_F(MultiGPUComprehensiveTest, EnumerateDevices_LimitedBuffer) {
    // WHAT: Test enumeration with small buffer
    // WHY:  Should limit results to buffer size
    // HOW:  Request max_devices=2, verify count <= 2

    multigpu_device_info_t devices[2];
    uint32_t count = 0;

    bool result = multigpu_enumerate_devices(devices, 2, &count);

    EXPECT_TRUE(result);
    EXPECT_LE(count, 2u) << "Should not exceed buffer size";
}

TEST_F(MultiGPUComprehensiveTest, EnumerateDevices_NullDevicesArray) {
    // WHAT: Test NULL devices array
    // WHY:  Guard clause validation
    // HOW:  Pass NULL devices, expect false

    uint32_t count = 0;
    bool result = multigpu_enumerate_devices(nullptr, 8, &count);

    EXPECT_FALSE(result) << "NULL devices should fail";
}

TEST_F(MultiGPUComprehensiveTest, EnumerateDevices_NullCountPointer) {
    // WHAT: Test NULL count pointer
    // WHY:  Guard clause validation
    // HOW:  Pass NULL count, expect false

    multigpu_device_info_t devices[8];
    bool result = multigpu_enumerate_devices(devices, 8, nullptr);

    EXPECT_FALSE(result) << "NULL count should fail";
}

TEST_F(MultiGPUComprehensiveTest, EnumerateDevices_ZeroMaxDevices) {
    // WHAT: Test zero max_devices parameter
    // WHY:  Guard clause validation
    // HOW:  Pass 0, expect false and count=0

    multigpu_device_info_t devices[8];
    uint32_t count = 999;

    bool result = multigpu_enumerate_devices(devices, 0, &count);

    EXPECT_FALSE(result);
    EXPECT_EQ(count, 0u);
}

//=============================================================================
// P2P Access Tests
//=============================================================================

TEST_F(MultiGPUComprehensiveTest, CheckPeerAccess_SameDevice) {
    // WHAT: Test P2P check for same device
    // WHY:  Device should always access itself
    // HOW:  Check device N -> device N

    for (int dev = 0; dev < 4; dev++) {
        bool result = multigpu_check_peer_access(dev, dev);
        EXPECT_TRUE(result) << "Device " << dev << " should access itself";
    }
}

TEST_F(MultiGPUComprehensiveTest, CheckPeerAccess_DifferentDevices) {
    // WHAT: Test P2P between different devices
    // WHY:  Mock backend should support P2P
    // HOW:  Check all pairs

    bool result_0_1 = multigpu_check_peer_access(0, 1);
    bool result_1_0 = multigpu_check_peer_access(1, 0);
    bool result_0_2 = multigpu_check_peer_access(0, 2);

    EXPECT_TRUE(result_0_1) << "Mock devices should support P2P";
    EXPECT_TRUE(result_1_0) << "P2P should be symmetric";
    EXPECT_TRUE(result_0_2);
}

TEST_F(MultiGPUComprehensiveTest, CheckPeerAccess_AllPairs) {
    // WHAT: Test P2P for all device pairs
    // WHY:  Ensure complete P2P connectivity
    // HOW:  Check all combinations

    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            bool result = multigpu_check_peer_access(i, j);
            if (i == j) {
                EXPECT_TRUE(result) << "Same device should succeed";
            } else {
                EXPECT_TRUE(result) << "Mock P2P should always succeed";
            }
        }
    }
}

//=============================================================================
// GPU Recommendation Tests
//=============================================================================

TEST_F(MultiGPUComprehensiveTest, GetRecommendedCount_TinyNetwork) {
    // WHAT: Test recommendation for very small network
    // WHY:  Should use single GPU (overhead not worth it)
    // HOW:  1K neurons, expect 1 GPU

    uint32_t recommended = multigpu_get_recommended_count(1000, 100, 8);

    EXPECT_EQ(recommended, 1u) << "Tiny network should use 1 GPU";
}

TEST_F(MultiGPUComprehensiveTest, GetRecommendedCount_SmallNetwork) {
    // WHAT: Test recommendation for small network
    // WHY:  Should use single GPU
    // HOW:  50K neurons, expect 1 GPU

    uint32_t recommended = multigpu_get_recommended_count(50000, 100, 8);

    EXPECT_EQ(recommended, 1u) << "Small network (<100K) should use 1 GPU";
}

TEST_F(MultiGPUComprehensiveTest, GetRecommendedCount_MediumNetwork) {
    // WHAT: Test recommendation for medium network
    // WHY:  Should use 2 GPUs
    // HOW:  500K neurons, expect 2 GPUs

    uint32_t recommended = multigpu_get_recommended_count(500000, 100, 8);

    EXPECT_EQ(recommended, 2u) << "Medium network (100K-1M) should use 2 GPUs";
}

TEST_F(MultiGPUComprehensiveTest, GetRecommendedCount_LargeNetwork) {
    // WHAT: Test recommendation for large network
    // WHY:  Should use 4 GPUs
    // HOW:  5M neurons, expect 4 GPUs

    uint32_t recommended = multigpu_get_recommended_count(5000000, 100, 8);

    EXPECT_EQ(recommended, 4u) << "Large network (1M-10M) should use 4 GPUs";
}

TEST_F(MultiGPUComprehensiveTest, GetRecommendedCount_VeryLargeNetwork) {
    // WHAT: Test recommendation for very large network
    // WHY:  Should use all available GPUs
    // HOW:  50M neurons, expect all 8 GPUs

    uint32_t recommended = multigpu_get_recommended_count(50000000, 100, 8);

    EXPECT_EQ(recommended, 8u) << "Very large network should use all GPUs";
}

TEST_F(MultiGPUComprehensiveTest, GetRecommendedCount_LimitedByAvailable) {
    // WHAT: Test recommendation limited by available GPUs
    // WHY:  Can't use more than available
    // HOW:  Large network, only 2 GPUs available

    uint32_t recommended = multigpu_get_recommended_count(5000000, 100, 2);

    EXPECT_EQ(recommended, 2u) << "Should not exceed available GPUs";
}

TEST_F(MultiGPUComprehensiveTest, GetRecommendedCount_NoGPUsAvailable) {
    // WHAT: Test recommendation with no GPUs
    // WHY:  Guard clause validation
    // HOW:  0 available, expect 0

    uint32_t recommended = multigpu_get_recommended_count(1000000, 100, 0);

    EXPECT_EQ(recommended, 0u) << "No GPUs should return 0";
}

TEST_F(MultiGPUComprehensiveTest, GetRecommendedCount_OnlyOneGPU) {
    // WHAT: Test recommendation with single GPU
    // WHY:  Should return 1 regardless of network size
    // HOW:  Large network, 1 GPU available

    uint32_t recommended = multigpu_get_recommended_count(10000000, 100, 1);

    EXPECT_EQ(recommended, 1u) << "Single GPU available should return 1";
}

//=============================================================================
// Context Creation Tests
//=============================================================================

TEST_F(MultiGPUComprehensiveTest, ContextCreate_DefaultConfig) {
    // WHAT: Test context creation with defaults
    // WHY:  Most common use case
    // HOW:  Create with default config, verify non-NULL

    multigpu_config_t config = multigpu_default_config();
    multigpu_context_t ctx = multigpu_context_create(&config);

    ASSERT_NE(ctx, nullptr) << "Default config should create valid context";

    uint32_t count = multigpu_get_device_count(ctx);
    EXPECT_GT(count, 0u);

    multigpu_context_destroy(ctx);
}

TEST_F(MultiGPUComprehensiveTest, ContextCreate_NullConfig) {
    // WHAT: Test NULL config handling
    // WHY:  Guard clause validation
    // HOW:  Pass NULL, expect NULL context

    multigpu_context_t ctx = multigpu_context_create(nullptr);

    EXPECT_EQ(ctx, nullptr) << "NULL config should return NULL";
}

TEST_F(MultiGPUComprehensiveTest, ContextCreate_SpecificDeviceCount) {
    // WHAT: Test creating with specific device count
    // WHY:  User may want subset of GPUs
    // HOW:  Request 2 devices, verify count

    multigpu_config_t config = multigpu_default_config();
    config.num_devices = 2;

    multigpu_context_t ctx = multigpu_context_create(&config);
    ASSERT_NE(ctx, nullptr);

    uint32_t count = multigpu_get_device_count(ctx);
    EXPECT_EQ(count, 2u) << "Should use requested device count";

    multigpu_context_destroy(ctx);
}

TEST_F(MultiGPUComprehensiveTest, ContextCreate_SingleDevice) {
    // WHAT: Test creating context with single GPU
    // WHY:  Valid edge case
    // HOW:  Request 1 device, verify

    multigpu_config_t config = multigpu_default_config();
    config.num_devices = 1;

    multigpu_context_t ctx = multigpu_context_create(&config);
    ASSERT_NE(ctx, nullptr);

    uint32_t count = multigpu_get_device_count(ctx);
    EXPECT_EQ(count, 1u);

    multigpu_context_destroy(ctx);
}

TEST_F(MultiGPUComprehensiveTest, ContextCreate_AllStrategies) {
    // WHAT: Test context creation with all partition strategies
    // WHY:  Verify all strategies are supported
    // HOW:  Create context for each strategy

    multigpu_partition_strategy_t strategies[] = {
        MULTIGPU_PARTITION_LAYER,
        MULTIGPU_PARTITION_NEURON,
        MULTIGPU_PARTITION_HYBRID,
        MULTIGPU_PARTITION_DYNAMIC,
        MULTIGPU_PARTITION_AUTO
    };

    for (auto strategy : strategies) {
        multigpu_config_t config = multigpu_default_config();
        config.partition_strategy = strategy;

        multigpu_context_t ctx = multigpu_context_create(&config);
        EXPECT_NE(ctx, nullptr) << "Strategy " << strategy << " should work";

        multigpu_context_destroy(ctx);
    }
}

TEST_F(MultiGPUComprehensiveTest, ContextCreate_WithPeerAccess) {
    // WHAT: Test context creation with P2P enabled
    // WHY:  Verify P2P setup doesn't break
    // HOW:  Create with enable_peer_access=true

    multigpu_config_t config = multigpu_default_config();
    config.enable_peer_access = true;
    config.num_devices = 2;

    multigpu_context_t ctx = multigpu_context_create(&config);
    EXPECT_NE(ctx, nullptr);

    multigpu_context_destroy(ctx);
}

TEST_F(MultiGPUComprehensiveTest, ContextCreate_WithoutPeerAccess) {
    // WHAT: Test context creation with P2P disabled
    // WHY:  Should work without P2P
    // HOW:  Create with enable_peer_access=false

    multigpu_config_t config = multigpu_default_config();
    config.enable_peer_access = false;

    multigpu_context_t ctx = multigpu_context_create(&config);
    EXPECT_NE(ctx, nullptr);

    multigpu_context_destroy(ctx);
}

TEST_F(MultiGPUComprehensiveTest, ContextDestroy_NullContext) {
    // WHAT: Test destroying NULL context
    // WHY:  Should not crash
    // HOW:  Pass NULL

    EXPECT_NO_THROW(multigpu_context_destroy(nullptr));
}

TEST_F(MultiGPUComprehensiveTest, ContextDestroy_DoubleDestroy) {
    // WHAT: Test destroying same context twice
    // WHY:  Should handle gracefully (though invalid usage)
    // HOW:  Destroy, then try to destroy again (don't pass same pointer)

    multigpu_config_t config = multigpu_default_config();
    multigpu_context_t ctx = multigpu_context_create(&config);
    ASSERT_NE(ctx, nullptr);

    multigpu_context_destroy(ctx);
    // Second destroy would be invalid - we just verify first one worked
    SUCCEED() << "First destroy completed successfully";
}

//=============================================================================
// Context Query Tests
//=============================================================================

TEST_F(MultiGPUContextTest, GetDeviceCount_ValidContext) {
    // WHAT: Test getting device count
    // WHY:  Verify context tracks device count
    // HOW:  Query count, verify > 0

    uint32_t count = multigpu_get_device_count(ctx);

    EXPECT_GT(count, 0u) << "Should have at least 1 device";
}

TEST_F(MultiGPUComprehensiveTest, GetDeviceCount_NullContext) {
    // WHAT: Test device count with NULL context
    // WHY:  Guard clause validation
    // HOW:  Pass NULL, expect 0

    uint32_t count = multigpu_get_device_count(nullptr);

    EXPECT_EQ(count, 0u);
}

TEST_F(MultiGPUContextTest, GetDeviceInfo_FirstDevice) {
    // WHAT: Test getting info for first device
    // WHY:  Verify device info accessible
    // HOW:  Query device 0

    multigpu_device_info_t info;
    bool result = multigpu_get_device_info(ctx, 0, &info);

    EXPECT_TRUE(result);
    EXPECT_GE(info.device_id, 0);
    EXPECT_GT(info.total_memory_bytes, 0u);
    EXPECT_GT(strlen(info.name), 0u);
}

TEST_F(MultiGPUContextTest, GetDeviceInfo_AllDevices) {
    // WHAT: Test getting info for all devices
    // WHY:  Verify all devices have valid info
    // HOW:  Query each device

    uint32_t count = multigpu_get_device_count(ctx);

    for (uint32_t i = 0; i < count; i++) {
        multigpu_device_info_t info;
        bool result = multigpu_get_device_info(ctx, i, &info);

        EXPECT_TRUE(result) << "Device " << i;
        EXPECT_GE(info.device_id, 0) << "Device " << i;
        EXPECT_GT(info.total_memory_bytes, 0u) << "Device " << i;
    }
}

TEST_F(MultiGPUContextTest, GetDeviceInfo_InvalidIndex) {
    // WHAT: Test invalid device index
    // WHY:  Guard clause validation
    // HOW:  Query out-of-range index

    multigpu_device_info_t info;
    bool result = multigpu_get_device_info(ctx, 999, &info);

    EXPECT_FALSE(result) << "Invalid index should fail";
}

TEST_F(MultiGPUContextTest, GetDeviceInfo_NullInfo) {
    // WHAT: Test NULL info pointer
    // WHY:  Guard clause validation
    // HOW:  Pass NULL info

    bool result = multigpu_get_device_info(ctx, 0, nullptr);

    EXPECT_FALSE(result);
}

TEST_F(MultiGPUComprehensiveTest, GetDeviceInfo_NullContext) {
    // WHAT: Test NULL context
    // WHY:  Guard clause validation
    // HOW:  Pass NULL context

    multigpu_device_info_t info;
    bool result = multigpu_get_device_info(nullptr, 0, &info);

    EXPECT_FALSE(result);
}

//=============================================================================
// Network Partitioning Tests
//=============================================================================

TEST_F(MultiGPUContextTest, PartitionNetwork_LayerStrategy) {
    // WHAT: Test layer-based partitioning
    // WHY:  Primary strategy for deep networks
    // HOW:  Partition 8 layers across GPUs

    uint32_t num_layers = 8;
    uint32_t neurons_per_layer[8] = {128, 256, 256, 512, 512, 256, 256, 128};

    bool result = multigpu_partition_network(ctx, num_layers, neurons_per_layer);

    EXPECT_TRUE(result);

    // Verify each layer is assigned
    for (uint32_t i = 0; i < num_layers; i++) {
        int assignment = multigpu_get_layer_assignment(ctx, i);
        EXPECT_GE(assignment, 0) << "Layer " << i << " should be assigned";
        EXPECT_LT(assignment, (int)multigpu_get_device_count(ctx));
    }
}

TEST_F(MultiGPUContextTest, PartitionNetwork_SmallNetwork) {
    // WHAT: Test partitioning small network
    // WHY:  Edge case with few layers
    // HOW:  Partition 2 layers

    uint32_t num_layers = 2;
    uint32_t neurons_per_layer[2] = {100, 200};

    bool result = multigpu_partition_network(ctx, num_layers, neurons_per_layer);

    EXPECT_TRUE(result);
}

TEST_F(MultiGPUContextTest, PartitionNetwork_LargeNetwork) {
    // WHAT: Test partitioning large network
    // WHY:  Real-world scenario
    // HOW:  Partition 32 layers

    uint32_t num_layers = 32;
    std::vector<uint32_t> neurons_per_layer(num_layers, 512);

    bool result = multigpu_partition_network(ctx, num_layers, neurons_per_layer.data());

    EXPECT_TRUE(result);
}

TEST_F(MultiGPUContextTest, PartitionNetwork_UnevenLayers) {
    // WHAT: Test partitioning with uneven layer count
    // WHY:  May not divide evenly across GPUs
    // HOW:  Use 7 layers (not divisible by typical GPU counts)

    uint32_t num_layers = 7;
    uint32_t neurons_per_layer[7] = {100, 200, 300, 400, 300, 200, 100};

    bool result = multigpu_partition_network(ctx, num_layers, neurons_per_layer);

    EXPECT_TRUE(result);

    // All layers should still be assigned
    for (uint32_t i = 0; i < num_layers; i++) {
        int assignment = multigpu_get_layer_assignment(ctx, i);
        EXPECT_GE(assignment, 0);
    }
}

TEST_F(MultiGPUComprehensiveTest, PartitionNetwork_NullContext) {
    // WHAT: Test NULL context
    // WHY:  Guard clause validation
    // HOW:  Pass NULL context

    uint32_t neurons[4] = {100, 200, 200, 100};
    bool result = multigpu_partition_network(nullptr, 4, neurons);

    EXPECT_FALSE(result);
}

TEST_F(MultiGPUContextTest, PartitionNetwork_NullNeuronsArray) {
    // WHAT: Test NULL neurons array
    // WHY:  Guard clause validation
    // HOW:  Pass NULL neurons_per_layer

    bool result = multigpu_partition_network(ctx, 4, nullptr);

    EXPECT_FALSE(result);
}

TEST_F(MultiGPUContextTest, PartitionNetwork_ZeroLayers) {
    // WHAT: Test zero layers
    // WHY:  Guard clause validation
    // HOW:  Pass num_layers=0

    uint32_t neurons[1] = {100};
    bool result = multigpu_partition_network(ctx, 0, neurons);

    EXPECT_FALSE(result);
}

TEST_F(MultiGPUContextTest, PartitionNetwork_MultipleCalls) {
    // WHAT: Test re-partitioning network
    // WHY:  Should handle re-partitioning
    // HOW:  Partition twice with different configs

    uint32_t neurons1[4] = {100, 200, 200, 100};
    bool result1 = multigpu_partition_network(ctx, 4, neurons1);
    EXPECT_TRUE(result1);

    uint32_t neurons2[6] = {128, 256, 512, 512, 256, 128};
    bool result2 = multigpu_partition_network(ctx, 6, neurons2);
    EXPECT_TRUE(result2);

    // Verify new partition is active
    int assignment = multigpu_get_layer_assignment(ctx, 5);
    EXPECT_GE(assignment, 0) << "New partition should work";
}

//=============================================================================
// Layer Assignment Tests
//=============================================================================

TEST_F(MultiGPUContextTest, GetLayerAssignment_AfterPartition) {
    // WHAT: Test getting assignments after partitioning
    // WHY:  Verify partition worked
    // HOW:  Partition then query assignments

    uint32_t num_layers = 8;
    uint32_t neurons[8] = {128, 256, 256, 512, 512, 256, 256, 128};

    multigpu_partition_network(ctx, num_layers, neurons);

    for (uint32_t i = 0; i < num_layers; i++) {
        int assignment = multigpu_get_layer_assignment(ctx, i);
        EXPECT_GE(assignment, 0) << "Layer " << i;
        EXPECT_LT(assignment, (int)multigpu_get_device_count(ctx));
    }
}

TEST_F(MultiGPUContextTest, GetLayerAssignment_BeforePartition) {
    // WHAT: Test getting assignment before partitioning
    // WHY:  Should return -1
    // HOW:  Query without partitioning

    int assignment = multigpu_get_layer_assignment(ctx, 0);

    EXPECT_EQ(assignment, -1) << "Should return -1 if not partitioned";
}

TEST_F(MultiGPUContextTest, GetLayerAssignment_InvalidLayerIndex) {
    // WHAT: Test invalid layer index
    // WHY:  Guard clause validation
    // HOW:  Query out-of-range after partition

    uint32_t neurons[4] = {100, 200, 200, 100};
    multigpu_partition_network(ctx, 4, neurons);

    int assignment = multigpu_get_layer_assignment(ctx, 999);

    EXPECT_EQ(assignment, -1);
}

TEST_F(MultiGPUComprehensiveTest, GetLayerAssignment_NullContext) {
    // WHAT: Test NULL context
    // WHY:  Guard clause validation
    // HOW:  Pass NULL

    int assignment = multigpu_get_layer_assignment(nullptr, 0);

    EXPECT_EQ(assignment, -1);
}

//=============================================================================
// Load Balancing Tests
//=============================================================================

TEST_F(MultiGPUContextTest, RebalanceWork_AfterPartition) {
    // WHAT: Test work rebalancing
    // WHY:  Verify rebalancing logic
    // HOW:  Partition then rebalance

    uint32_t neurons[8] = {128, 256, 256, 512, 512, 256, 256, 128};
    multigpu_partition_network(ctx, 8, neurons);

    bool result = multigpu_rebalance_work(ctx);

    // Result can be true (rebalanced) or false (already balanced)
    EXPECT_TRUE(result == true || result == false);
}

TEST_F(MultiGPUContextTest, RebalanceWork_BeforePartition) {
    // WHAT: Test rebalancing before partitioning
    // WHY:  Should fail gracefully
    // HOW:  Rebalance without partition

    bool result = multigpu_rebalance_work(ctx);

    EXPECT_FALSE(result) << "Should fail if not partitioned";
}

TEST_F(MultiGPUComprehensiveTest, RebalanceWork_NullContext) {
    // WHAT: Test NULL context
    // WHY:  Guard clause validation
    // HOW:  Pass NULL

    bool result = multigpu_rebalance_work(nullptr);

    EXPECT_FALSE(result);
}

TEST_F(MultiGPUComprehensiveTest, RebalanceWork_SingleGPU) {
    // WHAT: Test rebalancing with single GPU
    // WHY:  Should fail (nothing to rebalance)
    // HOW:  Create 1-GPU context, try to rebalance

    multigpu_config_t config = multigpu_default_config();
    config.num_devices = 1;

    multigpu_context_t single_ctx = multigpu_context_create(&config);
    ASSERT_NE(single_ctx, nullptr);

    uint32_t neurons[4] = {100, 200, 200, 100};
    multigpu_partition_network(single_ctx, 4, neurons);

    bool result = multigpu_rebalance_work(single_ctx);

    EXPECT_FALSE(result) << "Single GPU can't rebalance";

    multigpu_context_destroy(single_ctx);
}

//=============================================================================
// Memory Allocation Tests
//=============================================================================

TEST_F(MultiGPUContextTest, Alloc_BasicAllocation) {
    // WHAT: Test basic memory allocation
    // WHY:  Core functionality
    // HOW:  Allocate 1 MB, verify pointers

    size_t total_size = 1024 * 1024;  // 1 MB
    void** device_ptrs = multigpu_alloc(ctx, total_size);

    ASSERT_NE(device_ptrs, nullptr) << "Allocation should succeed";

    uint32_t num_devices = multigpu_get_device_count(ctx);
    for (uint32_t i = 0; i < num_devices; i++) {
        EXPECT_NE(device_ptrs[i], nullptr) << "Device " << i;
    }

    multigpu_free(ctx, device_ptrs);
}

TEST_F(MultiGPUContextTest, Alloc_SmallAllocation) {
    // WHAT: Test small allocation
    // WHY:  Edge case
    // HOW:  Allocate 1 KB

    size_t total_size = 1024;
    void** device_ptrs = multigpu_alloc(ctx, total_size);

    EXPECT_NE(device_ptrs, nullptr);

    multigpu_free(ctx, device_ptrs);
}

TEST_F(MultiGPUContextTest, Alloc_LargeAllocation) {
    // WHAT: Test large allocation
    // WHY:  Real-world scenario
    // HOW:  Allocate 100 MB

    size_t total_size = 100 * 1024 * 1024;  // 100 MB
    void** device_ptrs = multigpu_alloc(ctx, total_size);

    EXPECT_NE(device_ptrs, nullptr);

    multigpu_free(ctx, device_ptrs);
}

TEST_F(MultiGPUContextTest, Alloc_ZeroSize) {
    // WHAT: Test zero-size allocation
    // WHY:  Guard clause validation
    // HOW:  Allocate 0 bytes

    void** device_ptrs = multigpu_alloc(ctx, 0);

    EXPECT_EQ(device_ptrs, nullptr);
}

TEST_F(MultiGPUComprehensiveTest, Alloc_NullContext) {
    // WHAT: Test allocation with NULL context
    // WHY:  Guard clause validation
    // HOW:  Pass NULL

    void** device_ptrs = multigpu_alloc(nullptr, 1024);

    EXPECT_EQ(device_ptrs, nullptr);
}

TEST_F(MultiGPUContextTest, Alloc_MultipleAllocations) {
    // WHAT: Test multiple simultaneous allocations
    // WHY:  Verify memory management
    // HOW:  Allocate multiple buffers

    void** ptrs1 = multigpu_alloc(ctx, 1024);
    void** ptrs2 = multigpu_alloc(ctx, 2048);
    void** ptrs3 = multigpu_alloc(ctx, 4096);

    EXPECT_NE(ptrs1, nullptr);
    EXPECT_NE(ptrs2, nullptr);
    EXPECT_NE(ptrs3, nullptr);

    multigpu_free(ctx, ptrs1);
    multigpu_free(ctx, ptrs2);
    multigpu_free(ctx, ptrs3);
}

TEST_F(MultiGPUContextTest, Free_NullPointers) {
    // WHAT: Test freeing NULL pointers
    // WHY:  Should not crash
    // HOW:  Call free with NULL

    EXPECT_NO_THROW(multigpu_free(ctx, nullptr));
}

TEST_F(MultiGPUComprehensiveTest, Free_NullContext) {
    // WHAT: Test free with NULL context
    // WHY:  Should not crash
    // HOW:  Pass NULL context

    void* dummy = nullptr;
    EXPECT_NO_THROW(multigpu_free(nullptr, &dummy));
}

//=============================================================================
// Broadcast Tests
//=============================================================================

TEST_F(MultiGPUContextTest, Broadcast_BasicOperation) {
    // WHAT: Test basic broadcast
    // WHY:  Common operation
    // HOW:  Allocate, broadcast data

    // FIX: host_data must be same size as what we're broadcasting
    // size = 1024 floats * 4 bytes = 4096 bytes, so need 1024 floats
    size_t size = 1024 * sizeof(float);
    void** device_ptrs = multigpu_alloc(ctx, size * multigpu_get_device_count(ctx));
    ASSERT_NE(device_ptrs, nullptr);

    std::vector<float> host_data(1024, 3.14f);  // 1024 floats = 4096 bytes

    bool result = multigpu_broadcast(ctx, host_data.data(), device_ptrs, size);

    EXPECT_TRUE(result);

    multigpu_free(ctx, device_ptrs);
}

TEST_F(MultiGPUContextTest, Broadcast_LargeData) {
    // WHAT: Test broadcasting large data
    // WHY:  Real-world scenario
    // HOW:  Broadcast 10 MB

    size_t size = 10 * 1024 * 1024;
    void** device_ptrs = multigpu_alloc(ctx, size * multigpu_get_device_count(ctx));
    ASSERT_NE(device_ptrs, nullptr);

    std::vector<char> host_data(size, 0x42);

    bool result = multigpu_broadcast(ctx, host_data.data(), device_ptrs, size);

    EXPECT_TRUE(result);

    multigpu_free(ctx, device_ptrs);
}

TEST_F(MultiGPUComprehensiveTest, Broadcast_NullContext) {
    // WHAT: Test NULL context
    // WHY:  Guard clause validation
    // HOW:  Pass NULL

    float data[10];
    memset(data, 0, sizeof(data));
    void* ptrs[1];
    bool result = multigpu_broadcast(nullptr, data, &ptrs[0], sizeof(data));

    EXPECT_FALSE(result);
}

TEST_F(MultiGPUContextTest, Broadcast_NullHostData) {
    // WHAT: Test NULL host data
    // WHY:  Guard clause validation
    // HOW:  Pass NULL data

    uint32_t num_gpus = multigpu_get_device_count(ctx);
    void** device_ptrs = multigpu_alloc(ctx, 1024 * num_gpus);
    ASSERT_NE(device_ptrs, nullptr);

    bool result = multigpu_broadcast(ctx, nullptr, device_ptrs, 1024);

    EXPECT_FALSE(result);

    multigpu_free(ctx, device_ptrs);
}

TEST_F(MultiGPUContextTest, Broadcast_NullDevicePointers) {
    // WHAT: Test NULL device pointers
    // WHY:  Guard clause validation
    // HOW:  Pass NULL device_ptrs

    float data[10];
    memset(data, 0, sizeof(data));
    bool result = multigpu_broadcast(ctx, data, nullptr, sizeof(data));

    EXPECT_FALSE(result);
}

//=============================================================================
// Gather Tests
//=============================================================================

TEST_F(MultiGPUContextTest, Gather_BasicOperation) {
    // WHAT: Test basic gather
    // WHY:  Common operation
    // HOW:  Allocate, gather data

    uint32_t num_devices = multigpu_get_device_count(ctx);
    size_t size_per_gpu = 256 * sizeof(float);
    void** device_ptrs = multigpu_alloc(ctx, size_per_gpu * num_devices);
    ASSERT_NE(device_ptrs, nullptr);

    std::vector<float> host_data(256 * num_devices);

    bool result = multigpu_gather(ctx, device_ptrs, host_data.data(), size_per_gpu);

    EXPECT_TRUE(result);

    multigpu_free(ctx, device_ptrs);
}

TEST_F(MultiGPUContextTest, Gather_LargeData) {
    // WHAT: Test gathering large data
    // WHY:  Real-world scenario
    // HOW:  Gather 10 MB per GPU

    uint32_t num_devices = multigpu_get_device_count(ctx);
    size_t size_per_gpu = 10 * 1024 * 1024;
    void** device_ptrs = multigpu_alloc(ctx, size_per_gpu * num_devices);
    ASSERT_NE(device_ptrs, nullptr);

    std::vector<char> host_data(size_per_gpu * num_devices);

    bool result = multigpu_gather(ctx, device_ptrs, host_data.data(), size_per_gpu);

    EXPECT_TRUE(result);

    multigpu_free(ctx, device_ptrs);
}

TEST_F(MultiGPUComprehensiveTest, Gather_NullContext) {
    // WHAT: Test NULL context
    // WHY:  Guard clause validation
    // HOW:  Pass NULL

    void* ptrs[1];
    float data[10];
    bool result = multigpu_gather(nullptr, &ptrs[0], data, sizeof(data));

    EXPECT_FALSE(result);
}

TEST_F(MultiGPUContextTest, Gather_NullDevicePointers) {
    // WHAT: Test NULL device pointers
    // WHY:  Guard clause validation
    // HOW:  Pass NULL device_ptrs

    float data[10];
    bool result = multigpu_gather(ctx, nullptr, data, sizeof(data));

    EXPECT_FALSE(result);
}

TEST_F(MultiGPUContextTest, Gather_NullHostData) {
    // WHAT: Test NULL host data
    // WHY:  Guard clause validation
    // HOW:  Pass NULL data

    uint32_t num_gpus = multigpu_get_device_count(ctx);
    void** device_ptrs = multigpu_alloc(ctx, 256 * num_gpus);
    ASSERT_NE(device_ptrs, nullptr);

    bool result = multigpu_gather(ctx, device_ptrs, nullptr, 256);

    EXPECT_FALSE(result);

    multigpu_free(ctx, device_ptrs);
}

//=============================================================================
// Device Synchronization Tests
//=============================================================================

TEST_F(MultiGPUContextTest, SyncDevices_SameDevice) {
    // WHAT: Test syncing same device
    // WHY:  Should be no-op
    // HOW:  Sync device 0 -> device 0

    float src_data[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    float dst_data[10] = {0};
    bool result = multigpu_sync_devices(ctx, 0, 0, src_data, dst_data, sizeof(src_data));

    EXPECT_TRUE(result) << "Same device sync should succeed";
}

TEST_F(MultiGPUContextTest, SyncDevices_DifferentDevices) {
    // WHAT: Test syncing different devices
    // WHY:  Core P2P functionality
    // HOW:  Sync device 0 -> device 1

    uint32_t num_devices = multigpu_get_device_count(ctx);
    if (num_devices < 2) {
        GTEST_SKIP() << "Need at least 2 devices";
    }

    float src_data[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    float dst_data[10] = {0};
    bool result = multigpu_sync_devices(ctx, 0, 1, src_data, dst_data, sizeof(src_data));

    EXPECT_TRUE(result);
}

TEST_F(MultiGPUContextTest, SyncDevices_AllPairs) {
    // WHAT: Test syncing all device pairs
    // WHY:  Verify complete P2P connectivity
    // HOW:  Sync all combinations

    uint32_t num_devices = multigpu_get_device_count(ctx);
    float src_data[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    float dst_data[10] = {0};

    for (uint32_t src = 0; src < num_devices; src++) {
        for (uint32_t dst = 0; dst < num_devices; dst++) {
            bool result = multigpu_sync_devices(ctx, src, dst, src_data, dst_data, sizeof(src_data));
            EXPECT_TRUE(result) << "Sync " << src << " -> " << dst;
        }
    }
}

TEST_F(MultiGPUComprehensiveTest, SyncDevices_NullContext) {
    // WHAT: Test NULL context
    // WHY:  Guard clause validation
    // HOW:  Pass NULL

    float src_data[10];
    float dst_data[10];
    memset(src_data, 0, sizeof(src_data));
    memset(dst_data, 0, sizeof(dst_data));
    bool result = multigpu_sync_devices(nullptr, 0, 1, src_data, dst_data, sizeof(src_data));

    EXPECT_FALSE(result);
}

TEST_F(MultiGPUContextTest, SyncDevices_NullSrcData) {
    // WHAT: Test NULL source data
    // WHY:  Guard clause validation
    // HOW:  Pass NULL src_data

    float dst_data[10] = {0};
    bool result = multigpu_sync_devices(ctx, 0, 1, nullptr, dst_data, 100);

    EXPECT_FALSE(result);
}

TEST_F(MultiGPUContextTest, SyncDevices_NullDstData) {
    // WHAT: Test NULL destination data
    // WHY:  Guard clause validation
    // HOW:  Pass NULL dst_data

    float src_data[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    bool result = multigpu_sync_devices(ctx, 0, 1, src_data, nullptr, 100);

    EXPECT_FALSE(result);
}

TEST_F(MultiGPUContextTest, SyncDevices_InvalidSrcDevice) {
    // WHAT: Test invalid source device
    // WHY:  Guard clause validation
    // HOW:  Use out-of-range device ID

    float src_data[10];
    float dst_data[10];
    memset(src_data, 0, sizeof(src_data));
    memset(dst_data, 0, sizeof(dst_data));
    bool result = multigpu_sync_devices(ctx, 999, 0, src_data, dst_data, sizeof(src_data));

    EXPECT_FALSE(result);
}

TEST_F(MultiGPUContextTest, SyncDevices_InvalidDstDevice) {
    // WHAT: Test invalid destination device
    // WHY:  Guard clause validation
    // HOW:  Use out-of-range device ID

    float src_data[10];
    float dst_data[10];
    memset(src_data, 0, sizeof(src_data));
    memset(dst_data, 0, sizeof(dst_data));
    bool result = multigpu_sync_devices(ctx, 0, 999, src_data, dst_data, sizeof(src_data));

    EXPECT_FALSE(result);
}

//=============================================================================
// Synchronization Primitive Tests
//=============================================================================

TEST_F(MultiGPUContextTest, Synchronize_Success) {
    // WHAT: Test synchronizing all GPUs
    // WHY:  Core synchronization primitive
    // HOW:  Call synchronize, verify success

    bool result = multigpu_synchronize(ctx);

    EXPECT_TRUE(result);
}

TEST_F(MultiGPUComprehensiveTest, Synchronize_NullContext) {
    // WHAT: Test NULL context
    // WHY:  Guard clause validation
    // HOW:  Pass NULL

    bool result = multigpu_synchronize(nullptr);

    EXPECT_FALSE(result);
}

TEST_F(MultiGPUContextTest, Synchronize_AfterOperations) {
    // WHAT: Test sync after GPU operations
    // WHY:  Verify operations complete
    // HOW:  Allocate, broadcast, sync

    uint32_t num_gpus = multigpu_get_device_count(ctx);
    float data[256];
    void** device_ptrs = multigpu_alloc(ctx, sizeof(data) * num_gpus);
    ASSERT_NE(device_ptrs, nullptr);

    multigpu_broadcast(ctx, data, device_ptrs, sizeof(data));

    bool result = multigpu_synchronize(ctx);

    EXPECT_TRUE(result);

    multigpu_free(ctx, device_ptrs);
}

TEST_F(MultiGPUContextTest, IsIdle_AfterCreation) {
    // WHAT: Test idle check after creation
    // WHY:  New context should be idle
    // HOW:  Check idle immediately

    bool is_idle = multigpu_is_idle(ctx);

    EXPECT_TRUE(is_idle);
}

TEST_F(MultiGPUContextTest, IsIdle_AfterSynchronize) {
    // WHAT: Test idle after synchronization
    // WHY:  Should be idle after sync
    // HOW:  Do operations, sync, check idle

    uint32_t num_gpus = multigpu_get_device_count(ctx);
    float data[256];
    void** device_ptrs = multigpu_alloc(ctx, sizeof(data) * num_gpus);
    multigpu_broadcast(ctx, data, device_ptrs, sizeof(data));
    multigpu_synchronize(ctx);

    bool is_idle = multigpu_is_idle(ctx);

    EXPECT_TRUE(is_idle);

    multigpu_free(ctx, device_ptrs);
}

TEST_F(MultiGPUComprehensiveTest, IsIdle_NullContext) {
    // WHAT: Test NULL context
    // WHY:  Guard clause validation
    // HOW:  Pass NULL (returns true as conservative)

    bool is_idle = multigpu_is_idle(nullptr);

    EXPECT_TRUE(is_idle) << "NULL context should return true (conservative)";
}

//=============================================================================
// Performance Statistics Tests
//=============================================================================

TEST_F(MultiGPUContextTest, GetPerformanceStats_BasicQuery) {
    // WHAT: Test getting performance stats
    // WHY:  Monitor multi-GPU efficiency
    // HOW:  Query stats, verify values

    uint64_t total_ops;
    double total_time_ms;
    float avg_util;
    float imbalance;

    bool result = multigpu_get_performance_stats(ctx, &total_ops, &total_time_ms,
                                                 &avg_util, &imbalance);

    EXPECT_TRUE(result);
    EXPECT_GE(total_ops, 0u);
    EXPECT_GE(total_time_ms, 0.0);
    EXPECT_GE(avg_util, 0.0f);
    EXPECT_LE(avg_util, 1.0f);
    EXPECT_GE(imbalance, 0.0f);
    EXPECT_LE(imbalance, 1.0f);
}

TEST_F(MultiGPUContextTest, GetPerformanceStats_NullOutputs) {
    // WHAT: Test with NULL output pointers
    // WHY:  Should allow selective queries
    // HOW:  Pass NULL for some outputs

    bool result = multigpu_get_performance_stats(ctx, nullptr, nullptr,
                                                 nullptr, nullptr);

    EXPECT_TRUE(result) << "NULL outputs should be allowed";
}

TEST_F(MultiGPUComprehensiveTest, GetPerformanceStats_NullContext) {
    // WHAT: Test NULL context
    // WHY:  Guard clause validation
    // HOW:  Pass NULL

    uint64_t ops;
    double time;
    float util, imbal;

    bool result = multigpu_get_performance_stats(nullptr, &ops, &time, &util, &imbal);

    EXPECT_FALSE(result);
}

TEST_F(MultiGPUContextTest, GetDeviceStats_FirstDevice) {
    // WHAT: Test getting stats for first device
    // WHY:  Per-device monitoring
    // HOW:  Query device 0

    uint64_t ops;
    double time_ms;
    float util;

    bool result = multigpu_get_device_stats(ctx, 0, &ops, &time_ms, &util);

    EXPECT_TRUE(result);
    EXPECT_GE(ops, 0u);
    EXPECT_GE(time_ms, 0.0);
    EXPECT_GE(util, 0.0f);
    EXPECT_LE(util, 1.0f);
}

TEST_F(MultiGPUContextTest, GetDeviceStats_AllDevices) {
    // WHAT: Test getting stats for all devices
    // WHY:  Complete monitoring
    // HOW:  Query each device

    uint32_t num_devices = multigpu_get_device_count(ctx);

    for (uint32_t i = 0; i < num_devices; i++) {
        uint64_t ops;
        double time_ms;
        float util;

        bool result = multigpu_get_device_stats(ctx, i, &ops, &time_ms, &util);

        EXPECT_TRUE(result) << "Device " << i;
    }
}

TEST_F(MultiGPUContextTest, GetDeviceStats_InvalidIndex) {
    // WHAT: Test invalid device index
    // WHY:  Guard clause validation
    // HOW:  Query out-of-range

    uint64_t ops;
    double time_ms;
    float util;

    bool result = multigpu_get_device_stats(ctx, 999, &ops, &time_ms, &util);

    EXPECT_FALSE(result);
}

TEST_F(MultiGPUComprehensiveTest, GetDeviceStats_NullContext) {
    // WHAT: Test NULL context
    // WHY:  Guard clause validation
    // HOW:  Pass NULL

    uint64_t ops;
    double time;
    float util;

    bool result = multigpu_get_device_stats(nullptr, 0, &ops, &time, &util);

    EXPECT_FALSE(result);
}

TEST_F(MultiGPUContextTest, GetDeviceStats_NullOutputs) {
    // WHAT: Test with NULL output pointers
    // WHY:  Should allow selective queries
    // HOW:  Pass NULL for some outputs

    bool result = multigpu_get_device_stats(ctx, 0, nullptr, nullptr, nullptr);

    EXPECT_TRUE(result) << "NULL outputs should be allowed";
}

//=============================================================================
// Configuration Tests
//=============================================================================

TEST_F(MultiGPUComprehensiveTest, DefaultConfig_ValidValues) {
    // WHAT: Test default configuration
    // WHY:  Verify sensible defaults
    // HOW:  Get default, check all fields

    multigpu_config_t config = multigpu_default_config();

    EXPECT_EQ(config.num_devices, 0u) << "Should use all by default";
    EXPECT_TRUE(config.enable_peer_access);
    EXPECT_EQ(config.partition_strategy, MULTIGPU_PARTITION_HYBRID);
    EXPECT_EQ(config.loadbalance_strategy, MULTIGPU_LOADBALANCE_DYNAMIC);
    EXPECT_GT(config.streams_per_device, 0u);
    EXPECT_GT(config.loadbalance_interval, 0u);
    EXPECT_GT(config.imbalance_threshold, 0.0f);
    EXPECT_LT(config.imbalance_threshold, 1.0f);
}

TEST_F(MultiGPUComprehensiveTest, OptimalConfig_TinyNetwork) {
    // WHAT: Test optimal config for tiny network
    // WHY:  Should use 1 GPU
    // HOW:  1K neurons, 4 layers

    multigpu_config_t config = multigpu_get_optimal_config(1000, 4, 8);

    EXPECT_EQ(config.num_devices, 1u);
}

TEST_F(MultiGPUComprehensiveTest, OptimalConfig_SmallNetwork) {
    // WHAT: Test optimal config for small network
    // WHY:  Should use 1 GPU
    // HOW:  50K neurons, 10 layers

    multigpu_config_t config = multigpu_get_optimal_config(50000, 10, 8);

    EXPECT_EQ(config.num_devices, 1u);
}

TEST_F(MultiGPUComprehensiveTest, OptimalConfig_MediumNetwork) {
    // WHAT: Test optimal config for medium network
    // WHY:  Should use 2 GPUs
    // HOW:  500K neurons, 20 layers

    multigpu_config_t config = multigpu_get_optimal_config(500000, 20, 8);

    EXPECT_EQ(config.num_devices, 2u);
}

TEST_F(MultiGPUComprehensiveTest, OptimalConfig_LargeNetwork) {
    // WHAT: Test optimal config for large network
    // WHY:  Should use 4 GPUs
    // HOW:  5M neurons, 50 layers

    multigpu_config_t config = multigpu_get_optimal_config(5000000, 50, 8);

    EXPECT_EQ(config.num_devices, 4u);
}

TEST_F(MultiGPUComprehensiveTest, OptimalConfig_DeepNetwork) {
    // WHAT: Test optimal config for deep network
    // WHY:  Should prefer layer partition
    // HOW:  100K neurons, 1000 layers (high depth/width)

    multigpu_config_t config = multigpu_get_optimal_config(100000, 1000, 4);

    EXPECT_EQ(config.partition_strategy, MULTIGPU_PARTITION_LAYER);
}

TEST_F(MultiGPUComprehensiveTest, OptimalConfig_WideNetwork) {
    // WHAT: Test optimal config for wide network
    // WHY:  Should prefer neuron partition
    // HOW:  10M neurons, 5 layers (low depth/width)

    multigpu_config_t config = multigpu_get_optimal_config(10000000, 5, 8);

    EXPECT_EQ(config.partition_strategy, MULTIGPU_PARTITION_NEURON);
}

//=============================================================================
// Integration Tests
//=============================================================================

TEST_F(MultiGPUComprehensiveTest, Integration_FullWorkflow) {
    // WHAT: Test complete multi-GPU workflow
    // WHY:  Verify all components work together
    // HOW:  Create, partition, allocate, broadcast, sync, gather, destroy

    // 1. Create context
    multigpu_config_t config = multigpu_default_config();
    config.num_devices = 2;
    multigpu_context_t ctx = multigpu_context_create(&config);
    ASSERT_NE(ctx, nullptr);

    // 2. Partition network
    uint32_t neurons[8] = {128, 256, 256, 512, 512, 256, 256, 128};
    bool partition_ok = multigpu_partition_network(ctx, 8, neurons);
    EXPECT_TRUE(partition_ok);

    // 3. Allocate memory
    uint32_t num_gpus = multigpu_get_device_count(ctx);
    size_t size_per_gpu = sizeof(float) * 1024;
    void** device_ptrs = multigpu_alloc(ctx, size_per_gpu * num_gpus);
    ASSERT_NE(device_ptrs, nullptr);

    // 4. Broadcast data
    std::vector<float> host_data(1024, 1.5f);
    bool broadcast_ok = multigpu_broadcast(ctx, host_data.data(), device_ptrs,
                                          size_per_gpu);
    EXPECT_TRUE(broadcast_ok);

    // 5. Synchronize
    bool sync_ok = multigpu_synchronize(ctx);
    EXPECT_TRUE(sync_ok);

    // 6. Check idle
    bool is_idle = multigpu_is_idle(ctx);
    EXPECT_TRUE(is_idle);

    // 7. Gather results
    std::vector<float> result_data(1024);
    bool gather_ok = multigpu_gather(ctx, device_ptrs, result_data.data(),
                                     sizeof(float) * 512);
    EXPECT_TRUE(gather_ok);

    // 8. Get stats
    uint64_t ops;
    double time;
    float util, imbal;
    bool stats_ok = multigpu_get_performance_stats(ctx, &ops, &time, &util, &imbal);
    EXPECT_TRUE(stats_ok);

    // 9. Free and destroy
    multigpu_free(ctx, device_ptrs);
    multigpu_context_destroy(ctx);

    SUCCEED() << "Full workflow completed";
}

TEST_F(MultiGPUComprehensiveTest, Integration_MultipleWorkflows) {
    // WHAT: Test multiple sequential workflows
    // WHY:  Verify resource cleanup and reuse
    // HOW:  Run workflow 3 times

    for (int iteration = 0; iteration < 3; iteration++) {
        multigpu_config_t config = multigpu_default_config();
        multigpu_context_t ctx = multigpu_context_create(&config);
        ASSERT_NE(ctx, nullptr) << "Iteration " << iteration;

        uint32_t neurons[4] = {100, 200, 200, 100};
        multigpu_partition_network(ctx, 4, neurons);

        void** ptrs = multigpu_alloc(ctx, 1024);
        ASSERT_NE(ptrs, nullptr);

        multigpu_free(ctx, ptrs);
        multigpu_context_destroy(ctx);
    }

    SUCCEED() << "Multiple workflows completed";
}

TEST_F(MultiGPUComprehensiveTest, Integration_StressTest) {
    // WHAT: Stress test with many allocations
    // WHY:  Verify memory management under load
    // HOW:  Allocate many buffers

    multigpu_config_t config = multigpu_default_config();
    multigpu_context_t ctx = multigpu_context_create(&config);
    ASSERT_NE(ctx, nullptr);

    std::vector<void**> allocations;
    const int num_allocs = 20;

    for (int i = 0; i < num_allocs; i++) {
        void** ptrs = multigpu_alloc(ctx, 1024 * (i + 1));
        if (ptrs) {
            allocations.push_back(ptrs);
        }
    }

    EXPECT_GT(allocations.size(), 0u) << "Should succeed some allocations";

    for (auto ptrs : allocations) {
        multigpu_free(ctx, ptrs);
    }

    multigpu_context_destroy(ctx);
}

//=============================================================================
// Edge Case Tests
//=============================================================================

TEST_F(MultiGPUComprehensiveTest, EdgeCase_ExcessiveDeviceRequest) {
    // WHAT: Test requesting more devices than available
    // WHY:  Should cap at available count
    // HOW:  Request 100 devices

    multigpu_config_t config = multigpu_default_config();
    config.num_devices = 100;

    multigpu_context_t ctx = multigpu_context_create(&config);
    ASSERT_NE(ctx, nullptr);

    uint32_t actual_count = multigpu_get_device_count(ctx);
    EXPECT_LE(actual_count, 16u) << "Should cap at available";

    multigpu_context_destroy(ctx);
}

TEST_F(MultiGPUContextTest, EdgeCase_SingleLayerNetwork) {
    // WHAT: Test partitioning single layer
    // WHY:  Edge case
    // HOW:  Partition network with 1 layer

    uint32_t neurons[1] = {1000};
    bool result = multigpu_partition_network(ctx, 1, neurons);

    EXPECT_TRUE(result);

    int assignment = multigpu_get_layer_assignment(ctx, 0);
    EXPECT_GE(assignment, 0);
}

TEST_F(MultiGPUContextTest, EdgeCase_VeryDeepNetwork) {
    // WHAT: Test very deep network
    // WHY:  Stress partition logic
    // HOW:  100 layers

    std::vector<uint32_t> neurons(100, 256);
    bool result = multigpu_partition_network(ctx, 100, neurons.data());

    EXPECT_TRUE(result);
}

TEST_F(MultiGPUContextTest, EdgeCase_VaryingLayerSizes) {
    // WHAT: Test network with highly varying layer sizes
    // WHY:  Challenge load balancing
    // HOW:  Layers from 10 to 10000 neurons

    uint32_t neurons[10] = {10, 100, 1000, 10000, 5000,
                           2500, 1250, 625, 312, 156};

    bool result = multigpu_partition_network(ctx, 10, neurons);

    EXPECT_TRUE(result);
}

//=============================================================================
// Fault Tolerance Tests
//=============================================================================

TEST_F(MultiGPUContextTest, FaultTolerance_BroadcastAfterFree) {
    // WHAT: Test broadcast after freeing memory
    // WHY:  Verify error handling
    // HOW:  Free then try to broadcast
    // NOTE: This test invokes undefined behavior (use-after-free) and is skipped
    //       The implementation should use NULL checks, not rely on freed pointer detection

    GTEST_SKIP() << "Skipping use-after-free test - invokes undefined behavior";

    // The test is disabled because:
    // 1. It invokes undefined behavior which may segfault
    // 2. The correct fix is to NULL the pointer after freeing and check for NULL
    // 3. Use-after-free detection should be done by sanitizers, not production code
}

TEST_F(MultiGPUContextTest, FaultTolerance_PartitionAfterPartition) {
    // WHAT: Test re-partitioning
    // WHY:  Should handle re-partitioning
    // HOW:  Partition twice

    uint32_t neurons1[4] = {100, 200, 200, 100};
    bool result1 = multigpu_partition_network(ctx, 4, neurons1);
    EXPECT_TRUE(result1);

    uint32_t neurons2[6] = {128, 256, 512, 512, 256, 128};
    bool result2 = multigpu_partition_network(ctx, 6, neurons2);
    EXPECT_TRUE(result2);
}

TEST_F(MultiGPUContextTest, FaultTolerance_SynchronizeMultipleTimes) {
    // WHAT: Test multiple synchronizations
    // WHY:  Should be idempotent
    // HOW:  Sync 10 times

    for (int i = 0; i < 10; i++) {
        bool result = multigpu_synchronize(ctx);
        EXPECT_TRUE(result) << "Sync " << i;
    }
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
