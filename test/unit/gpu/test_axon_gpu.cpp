/**
 * @file test_axon_gpu.cpp
 * @brief Unit tests for GPU-accelerated axon module
 *
 * Tests axon_gpu_* APIs for tensor-based GPU axon simulation
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <vector>

// Include GPU headers before extern "C"
#include "gpu/axon/nimcp_axon_gpu.h"
#include "gpu/context/nimcp_gpu_context.h"

// Headers have their own extern "C" guards
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Constants
//=============================================================================

static const uint32_t TEST_NUM_AXONS = 256;
static const uint32_t TEST_NUM_SEGMENTS = 16;
static const uint32_t SMALL_NETWORK = 32;
static const uint32_t LARGE_NETWORK = 4096;

//=============================================================================
// Test Fixture
//=============================================================================

class AxonGPUTest : public ::testing::Test {
protected:
    nimcp_gpu_context_t* gpu_ctx = nullptr;
    axon_gpu_context_t* axon_ctx = nullptr;
    bool gpu_available = false;

    void SetUp() override {
        gpu_ctx = nimcp_gpu_context_create(0);
        gpu_available = (gpu_ctx != nullptr);
    }

    void TearDown() override {
        if (axon_ctx) {
            axon_gpu_destroy(axon_ctx);
            axon_ctx = nullptr;
        }
        if (gpu_ctx) {
            nimcp_gpu_context_destroy(gpu_ctx);
            gpu_ctx = nullptr;
        }
    }

    void CreateAxonContext(uint32_t num_axons = TEST_NUM_AXONS,
                           uint32_t num_segments = TEST_NUM_SEGMENTS) {
        if (!gpu_available) {
            GTEST_SKIP() << "GPU not available";
        }

        axon_gpu_config_t config = axon_gpu_default_config();
        config.max_axons = num_axons * 2;
        config.max_segments = num_segments;
        axon_ctx = axon_gpu_create(gpu_ctx, &config);
        ASSERT_NE(axon_ctx, nullptr) << "Failed to create axon context";

        ASSERT_TRUE(axon_gpu_init_tensors(axon_ctx, num_axons, num_segments))
            << "Failed to init tensors";
    }
};

//=============================================================================
// Configuration Tests
//=============================================================================

TEST_F(AxonGPUTest, DefaultConfig_HasValidValues) {
    axon_gpu_config_t config = axon_gpu_default_config();

    EXPECT_GT(config.max_axons, 0);
    EXPECT_GT(config.max_segments, 0);
    EXPECT_GT(config.refractory_period_ms, 0.0f);
    EXPECT_GT(config.base_velocity_ms, 0.0f);
    EXPECT_GT(config.myelin_multiplier, 1.0f);
}

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(AxonGPUTest, Create_NullContext_ReturnsNull) {
    axon_gpu_config_t config = axon_gpu_default_config();
    axon_gpu_context_t* ctx = axon_gpu_create(nullptr, &config);
    EXPECT_EQ(ctx, nullptr);
}

TEST_F(AxonGPUTest, Create_NullConfig_UsesDefaults) {
    if (!gpu_available) GTEST_SKIP() << "GPU not available";

    axon_gpu_context_t* ctx = axon_gpu_create(gpu_ctx, nullptr);
    EXPECT_NE(ctx, nullptr);
    axon_gpu_destroy(ctx);
}

TEST_F(AxonGPUTest, Create_ValidConfig_Succeeds) {
    if (!gpu_available) GTEST_SKIP() << "GPU not available";

    axon_gpu_config_t config = axon_gpu_default_config();
    config.max_axons = 1024;
    config.max_segments = 8;

    axon_gpu_context_t* ctx = axon_gpu_create(gpu_ctx, &config);
    EXPECT_NE(ctx, nullptr);
    axon_gpu_destroy(ctx);
}

TEST_F(AxonGPUTest, Destroy_Null_DoesNotCrash) {
    axon_gpu_destroy(nullptr);
    SUCCEED();
}

TEST_F(AxonGPUTest, Synchronize_ValidContext_Succeeds) {
    CreateAxonContext();
    EXPECT_TRUE(axon_gpu_synchronize(axon_ctx));
}

TEST_F(AxonGPUTest, Synchronize_NullContext_ReturnsFalse) {
    EXPECT_FALSE(axon_gpu_synchronize(nullptr));
}

//=============================================================================
// Tensor Initialization Tests
//=============================================================================

TEST_F(AxonGPUTest, InitTensors_ValidDimensions_Succeeds) {
    if (!gpu_available) GTEST_SKIP() << "GPU not available";

    axon_gpu_config_t config = axon_gpu_default_config();
    axon_ctx = axon_gpu_create(gpu_ctx, &config);
    ASSERT_NE(axon_ctx, nullptr);

    EXPECT_TRUE(axon_gpu_init_tensors(axon_ctx, 128, 16));
}

TEST_F(AxonGPUTest, InitTensors_ZeroAxons_ReturnsFalse) {
    if (!gpu_available) GTEST_SKIP() << "GPU not available";

    axon_gpu_config_t config = axon_gpu_default_config();
    axon_ctx = axon_gpu_create(gpu_ctx, &config);
    ASSERT_NE(axon_ctx, nullptr);

    EXPECT_FALSE(axon_gpu_init_tensors(axon_ctx, 0, 16));
}

TEST_F(AxonGPUTest, InitTensors_ZeroSegments_ReturnsFalse) {
    if (!gpu_available) GTEST_SKIP() << "GPU not available";

    axon_gpu_config_t config = axon_gpu_default_config();
    axon_ctx = axon_gpu_create(gpu_ctx, &config);
    ASSERT_NE(axon_ctx, nullptr);

    EXPECT_FALSE(axon_gpu_init_tensors(axon_ctx, 128, 0));
}

//=============================================================================
// Property Upload Tests
//=============================================================================

TEST_F(AxonGPUTest, UploadProperties_ValidData_Succeeds) {
    CreateAxonContext();

    std::vector<float> diameters(TEST_NUM_AXONS, 1.0f);
    std::vector<float> lengths(TEST_NUM_AXONS, 100.0f);
    std::vector<float> myelination(TEST_NUM_AXONS * TEST_NUM_SEGMENTS, 0.5f);

    EXPECT_TRUE(axon_gpu_upload_properties(
        axon_ctx, diameters.data(), lengths.data(),
        myelination.data(), TEST_NUM_AXONS));
}

TEST_F(AxonGPUTest, UploadProperties_NullDiameters_ReturnsFalse) {
    CreateAxonContext();

    std::vector<float> lengths(TEST_NUM_AXONS, 100.0f);
    std::vector<float> myelination(TEST_NUM_AXONS * TEST_NUM_SEGMENTS, 0.5f);

    EXPECT_FALSE(axon_gpu_upload_properties(
        axon_ctx, nullptr, lengths.data(),
        myelination.data(), TEST_NUM_AXONS));
}

TEST_F(AxonGPUTest, UploadConnectivity_ValidData_Succeeds) {
    CreateAxonContext();

    std::vector<uint32_t> sources(TEST_NUM_AXONS);
    std::vector<uint32_t> targets(TEST_NUM_AXONS);
    for (uint32_t i = 0; i < TEST_NUM_AXONS; i++) {
        sources[i] = i;
        targets[i] = (i + 1) % TEST_NUM_AXONS;
    }

    EXPECT_TRUE(axon_gpu_upload_connectivity(
        axon_ctx, sources.data(), targets.data(), TEST_NUM_AXONS));
}

//=============================================================================
// Signal Propagation Tests
//=============================================================================

TEST_F(AxonGPUTest, Propagate_ValidContext_Succeeds) {
    CreateAxonContext();

    // Upload properties first
    std::vector<float> diameters(TEST_NUM_AXONS, 1.0f);
    std::vector<float> lengths(TEST_NUM_AXONS, 100.0f);
    std::vector<float> myelination(TEST_NUM_AXONS * TEST_NUM_SEGMENTS, 0.5f);
    axon_gpu_upload_properties(axon_ctx, diameters.data(), lengths.data(),
                               myelination.data(), TEST_NUM_AXONS);

    EXPECT_TRUE(axon_gpu_propagate(axon_ctx, 0.1f));
}

TEST_F(AxonGPUTest, Propagate_NullContext_ReturnsFalse) {
    EXPECT_FALSE(axon_gpu_propagate(nullptr, 0.1f));
}

TEST_F(AxonGPUTest, InitiateSpikes_BatchSpikes_Succeeds) {
    CreateAxonContext();

    std::vector<uint32_t> indices = {0, 5, 10, 15, 20};
    std::vector<float> amplitudes = {1.0f, 0.8f, 0.9f, 1.0f, 0.7f};

    EXPECT_TRUE(axon_gpu_initiate_spikes(
        axon_ctx, indices.data(), amplitudes.data(),
        indices.size(), 1000));
}

TEST_F(AxonGPUTest, InitiateSpikes_NullAmplitudes_UsesDefaults) {
    CreateAxonContext();

    std::vector<uint32_t> indices = {0, 1, 2, 3, 4};

    EXPECT_TRUE(axon_gpu_initiate_spikes(
        axon_ctx, indices.data(), nullptr, indices.size(), 1000));
}

TEST_F(AxonGPUTest, CheckArrivals_ValidContext_Succeeds) {
    CreateAxonContext();

    std::vector<uint32_t> arrived(TEST_NUM_AXONS);
    uint32_t count = 0;

    EXPECT_TRUE(axon_gpu_check_arrivals(
        axon_ctx, 5000, arrived.data(), TEST_NUM_AXONS, &count));
    EXPECT_LE(count, TEST_NUM_AXONS);
}

//=============================================================================
// Velocity and Myelination Tests
//=============================================================================

TEST_F(AxonGPUTest, UpdateVelocities_ValidContext_Succeeds) {
    CreateAxonContext();

    // Upload properties first
    std::vector<float> diameters(TEST_NUM_AXONS, 1.0f);
    std::vector<float> lengths(TEST_NUM_AXONS, 100.0f);
    std::vector<float> myelination(TEST_NUM_AXONS * TEST_NUM_SEGMENTS, 0.5f);
    axon_gpu_upload_properties(axon_ctx, diameters.data(), lengths.data(),
                               myelination.data(), TEST_NUM_AXONS);

    EXPECT_TRUE(axon_gpu_update_velocities(axon_ctx));
}

TEST_F(AxonGPUTest, ApplyMyelination_ValidContext_Succeeds) {
    CreateAxonContext();

    EXPECT_TRUE(axon_gpu_apply_myelination(axon_ctx));
}

TEST_F(AxonGPUTest, UpdateMyelination_BatchUpdate_Succeeds) {
    CreateAxonContext();

    std::vector<uint32_t> axon_indices = {0, 1, 2};
    std::vector<uint32_t> seg_indices = {0, 1, 2};
    std::vector<float> new_myelin = {0.8f, 0.7f, 0.9f};

    EXPECT_TRUE(axon_gpu_update_myelination(
        axon_ctx, axon_indices.data(), seg_indices.data(),
        new_myelin.data(), 3));
}

//=============================================================================
// Refractory Period Tests
//=============================================================================

TEST_F(AxonGPUTest, UpdateRefractory_ValidContext_Succeeds) {
    CreateAxonContext();
    EXPECT_TRUE(axon_gpu_update_refractory(axon_ctx, 0.1f));
}

TEST_F(AxonGPUTest, GetAvailable_ValidContext_Succeeds) {
    CreateAxonContext();

    std::vector<uint8_t> available(TEST_NUM_AXONS);
    EXPECT_TRUE(axon_gpu_get_available(axon_ctx, available.data(), TEST_NUM_AXONS));

    // Initially all should be available (not refractory)
    uint32_t available_count = 0;
    for (uint8_t a : available) {
        if (a) available_count++;
    }
    EXPECT_GT(available_count, 0);
}

//=============================================================================
// Activity and ATP Tests
//=============================================================================

TEST_F(AxonGPUTest, UpdateActivity_ValidContext_Succeeds) {
    CreateAxonContext();
    EXPECT_TRUE(axon_gpu_update_activity(axon_ctx, 0.1f));
}

TEST_F(AxonGPUTest, UpdateATP_ValidContext_Succeeds) {
    CreateAxonContext();
    EXPECT_TRUE(axon_gpu_update_atp(axon_ctx, 0.1f, 0.01f));
}

//=============================================================================
// Batch Step Tests
//=============================================================================

TEST_F(AxonGPUTest, Step_ValidContext_Succeeds) {
    CreateAxonContext();

    // Upload properties
    std::vector<float> diameters(TEST_NUM_AXONS, 1.0f);
    std::vector<float> lengths(TEST_NUM_AXONS, 100.0f);
    std::vector<float> myelination(TEST_NUM_AXONS * TEST_NUM_SEGMENTS, 0.5f);
    axon_gpu_upload_properties(axon_ctx, diameters.data(), lengths.data(),
                               myelination.data(), TEST_NUM_AXONS);

    EXPECT_TRUE(axon_gpu_step(axon_ctx, 0.1f, 1000));
}

TEST_F(AxonGPUTest, Step_MultipleSteps_Stable) {
    CreateAxonContext();

    std::vector<float> diameters(TEST_NUM_AXONS, 1.0f);
    std::vector<float> lengths(TEST_NUM_AXONS, 100.0f);
    std::vector<float> myelination(TEST_NUM_AXONS * TEST_NUM_SEGMENTS, 0.5f);
    axon_gpu_upload_properties(axon_ctx, diameters.data(), lengths.data(),
                               myelination.data(), TEST_NUM_AXONS);

    for (int i = 0; i < 100; i++) {
        EXPECT_TRUE(axon_gpu_step(axon_ctx, 0.1f, i * 100));
    }
}

//=============================================================================
// Data Retrieval Tests
//=============================================================================

TEST_F(AxonGPUTest, GetVelocities_ValidContext_Succeeds) {
    CreateAxonContext();

    std::vector<float> diameters(TEST_NUM_AXONS, 1.0f);
    std::vector<float> lengths(TEST_NUM_AXONS, 100.0f);
    std::vector<float> myelination(TEST_NUM_AXONS * TEST_NUM_SEGMENTS, 0.5f);
    axon_gpu_upload_properties(axon_ctx, diameters.data(), lengths.data(),
                               myelination.data(), TEST_NUM_AXONS);
    axon_gpu_update_velocities(axon_ctx);

    std::vector<float> velocities(TEST_NUM_AXONS);
    EXPECT_TRUE(axon_gpu_get_velocities(axon_ctx, velocities.data(), TEST_NUM_AXONS));
}

TEST_F(AxonGPUTest, GetDelays_ValidContext_Succeeds) {
    CreateAxonContext();

    std::vector<float> delays(TEST_NUM_AXONS);
    EXPECT_TRUE(axon_gpu_get_delays(axon_ctx, delays.data(), TEST_NUM_AXONS));
}

TEST_F(AxonGPUTest, GetMyelination_ValidContext_Succeeds) {
    CreateAxonContext();

    std::vector<float> myelination(TEST_NUM_AXONS * TEST_NUM_SEGMENTS);
    EXPECT_TRUE(axon_gpu_get_myelination(axon_ctx, myelination.data(), TEST_NUM_AXONS));
}

TEST_F(AxonGPUTest, GetSignals_ValidContext_Succeeds) {
    CreateAxonContext();

    std::vector<float> signals(TEST_NUM_AXONS * TEST_NUM_SEGMENTS);
    EXPECT_TRUE(axon_gpu_get_signals(axon_ctx, signals.data(), TEST_NUM_AXONS));
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(AxonGPUTest, GetStats_ValidContext_Succeeds) {
    CreateAxonContext();

    axon_gpu_stats_t stats;
    EXPECT_TRUE(axon_gpu_get_stats(axon_ctx, &stats));
    EXPECT_GE(stats.total_spikes, 0u);
}

TEST_F(AxonGPUTest, GetStats_NullStats_ReturnsFalse) {
    CreateAxonContext();
    EXPECT_FALSE(axon_gpu_get_stats(axon_ctx, nullptr));
}

TEST_F(AxonGPUTest, ResetStats_ValidContext_Succeeds) {
    CreateAxonContext();
    axon_gpu_reset_stats(axon_ctx);

    axon_gpu_stats_t stats;
    axon_gpu_get_stats(axon_ctx, &stats);
    EXPECT_EQ(stats.total_spikes, 0u);
}

//=============================================================================
// CPU Reference Tests
//=============================================================================

TEST_F(AxonGPUTest, CPUPropagate_ValidData_Succeeds) {
    std::vector<float> signals(SMALL_NETWORK * TEST_NUM_SEGMENTS, 0.5f);
    std::vector<float> velocities(SMALL_NETWORK * TEST_NUM_SEGMENTS, 10.0f);

    EXPECT_TRUE(axon_cpu_propagate(
        signals.data(), velocities.data(),
        SMALL_NETWORK, TEST_NUM_SEGMENTS, 0.1f));
}

TEST_F(AxonGPUTest, CPUCalculateVelocities_ValidData_Succeeds) {
    std::vector<float> velocities(SMALL_NETWORK);
    std::vector<float> diameters(SMALL_NETWORK, 1.0f);
    std::vector<float> myelination(SMALL_NETWORK, 0.5f);

    EXPECT_TRUE(axon_cpu_calculate_velocities(
        velocities.data(), diameters.data(), myelination.data(),
        SMALL_NETWORK, 1.0f, 5.0f));

    // Velocities should be positive
    for (float v : velocities) {
        EXPECT_GT(v, 0.0f);
    }
}

TEST_F(AxonGPUTest, CPUApplyMyelination_ValidData_Succeeds) {
    std::vector<float> velocities(SMALL_NETWORK, 1.0f);
    std::vector<float> myelination(SMALL_NETWORK, 0.5f);

    EXPECT_TRUE(axon_cpu_apply_myelination(
        velocities.data(), myelination.data(),
        SMALL_NETWORK, 5.0f, 100.0f));
}

//=============================================================================
// Large Network Tests
//=============================================================================

TEST_F(AxonGPUTest, LargeNetwork_CreateAndStep_Succeeds) {
    if (!gpu_available) GTEST_SKIP() << "GPU not available";

    axon_gpu_config_t config = axon_gpu_default_config();
    config.max_axons = LARGE_NETWORK * 2;
    config.max_segments = TEST_NUM_SEGMENTS;

    axon_ctx = axon_gpu_create(gpu_ctx, &config);
    ASSERT_NE(axon_ctx, nullptr);
    ASSERT_TRUE(axon_gpu_init_tensors(axon_ctx, LARGE_NETWORK, TEST_NUM_SEGMENTS));

    // Upload properties
    std::vector<float> diameters(LARGE_NETWORK, 1.0f);
    std::vector<float> lengths(LARGE_NETWORK, 100.0f);
    std::vector<float> myelination(LARGE_NETWORK * TEST_NUM_SEGMENTS, 0.5f);
    axon_gpu_upload_properties(axon_ctx, diameters.data(), lengths.data(),
                               myelination.data(), LARGE_NETWORK);

    // Run step
    EXPECT_TRUE(axon_gpu_step(axon_ctx, 0.1f, 1000));
}
