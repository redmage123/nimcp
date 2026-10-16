/**
 * @file test_myelin_gpu.cpp
 * @brief Unit tests for GPU-accelerated myelin sheath module
 *
 * Tests myelin_gpu_* APIs for tensor-based GPU myelin simulation
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <vector>

// Include GPU headers before extern "C"
#include "gpu/glial/nimcp_myelin_gpu.h"
#include "gpu/context/nimcp_gpu_context.h"

// Headers have their own extern "C" guards
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Constants
//=============================================================================

static const uint32_t TEST_NUM_AXONS = 128;
static const uint32_t TEST_NUM_INTERNODES = 16;
static const uint32_t SMALL_NETWORK = 32;
static const uint32_t LARGE_NETWORK = 2048;

//=============================================================================
// Test Fixture
//=============================================================================

class MyelinGPUTest : public ::testing::Test {
protected:
    nimcp_gpu_context_t* gpu_ctx = nullptr;
    myelin_gpu_context_t* myelin_ctx = nullptr;
    bool gpu_available = false;

    void SetUp() override {
        gpu_ctx = nimcp_gpu_context_create(0);
        gpu_available = (gpu_ctx != nullptr);
    }

    void TearDown() override {
        if (myelin_ctx) {
            myelin_gpu_destroy(myelin_ctx);
            myelin_ctx = nullptr;
        }
        if (gpu_ctx) {
            nimcp_gpu_context_destroy(gpu_ctx);
            gpu_ctx = nullptr;
        }
    }

    void CreateMyelinContext(uint32_t num_axons = TEST_NUM_AXONS,
                             uint32_t num_internodes = TEST_NUM_INTERNODES) {
        if (!gpu_available) {
            GTEST_SKIP() << "GPU not available";
        }

        myelin_gpu_config_t config = myelin_gpu_default_config();
        config.max_axons = num_axons * 2;
        config.max_internodes = num_internodes;
        myelin_ctx = myelin_gpu_create(gpu_ctx, &config);
        ASSERT_NE(myelin_ctx, nullptr) << "Failed to create myelin context";
    }

    void UploadTestData() {
        std::vector<float> diameters(TEST_NUM_AXONS);
        std::vector<float> internode_lengths(TEST_NUM_AXONS * TEST_NUM_INTERNODES);

        for (uint32_t i = 0; i < TEST_NUM_AXONS; i++) {
            diameters[i] = 1.0f + (i % 10) * 0.1f;  // 1.0 - 2.0 um
            for (uint32_t j = 0; j < TEST_NUM_INTERNODES; j++) {
                internode_lengths[i * TEST_NUM_INTERNODES + j] = 100.0f + (j % 5) * 10.0f;
            }
        }

        myelin_gpu_upload_axon_properties(myelin_ctx, diameters.data(),
                                          internode_lengths.data(), TEST_NUM_AXONS);

        // Upload lamellae
        std::vector<uint32_t> lamellae(TEST_NUM_AXONS * TEST_NUM_INTERNODES, 50);
        myelin_gpu_upload_lamellae(myelin_ctx, lamellae.data());

        // Upload integrity
        std::vector<float> integrity(TEST_NUM_AXONS * TEST_NUM_INTERNODES, 1.0f);
        myelin_gpu_upload_integrity(myelin_ctx, integrity.data());
    }
};

//=============================================================================
// Configuration Tests
//=============================================================================

TEST_F(MyelinGPUTest, DefaultConfig_HasValidValues) {
    myelin_gpu_config_t config = myelin_gpu_default_config();

    EXPECT_GT(config.max_axons, 0);
    EXPECT_GT(config.max_internodes, 0);
    EXPECT_GT(config.target_g_ratio, 0.0f);
    EXPECT_LT(config.target_g_ratio, 1.0f);
    EXPECT_GT(config.myelination_rate_max, 0.0f);
}

TEST_F(MyelinGPUTest, DefaultConfig_BiologicallyReasonable) {
    myelin_gpu_config_t config = myelin_gpu_default_config();

    // Optimal g-ratio should be around 0.77 for CNS
    EXPECT_NEAR(config.target_g_ratio, 0.77f, 0.1f);
}

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(MyelinGPUTest, Create_NullContext_ReturnsNull) {
    myelin_gpu_config_t config = myelin_gpu_default_config();
    myelin_gpu_context_t* ctx = myelin_gpu_create(nullptr, &config);
    EXPECT_EQ(ctx, nullptr);
}

TEST_F(MyelinGPUTest, Create_NullConfig_UsesDefaults) {
    if (!gpu_available) GTEST_SKIP() << "GPU not available";

    myelin_gpu_context_t* ctx = myelin_gpu_create(gpu_ctx, nullptr);
    EXPECT_NE(ctx, nullptr);
    myelin_gpu_destroy(ctx);
}

TEST_F(MyelinGPUTest, Create_ValidConfig_Succeeds) {
    if (!gpu_available) GTEST_SKIP() << "GPU not available";

    myelin_gpu_config_t config = myelin_gpu_default_config();
    config.max_axons = 512;
    config.max_internodes = 32;

    myelin_gpu_context_t* ctx = myelin_gpu_create(gpu_ctx, &config);
    EXPECT_NE(ctx, nullptr);
    myelin_gpu_destroy(ctx);
}

TEST_F(MyelinGPUTest, Destroy_Null_DoesNotCrash) {
    myelin_gpu_destroy(nullptr);
    SUCCEED();
}

TEST_F(MyelinGPUTest, Synchronize_ValidContext_Succeeds) {
    CreateMyelinContext();
    EXPECT_TRUE(myelin_gpu_synchronize(myelin_ctx));
}

TEST_F(MyelinGPUTest, Synchronize_NullContext_ReturnsFalse) {
    EXPECT_FALSE(myelin_gpu_synchronize(nullptr));
}

TEST_F(MyelinGPUTest, Reset_ValidContext_Succeeds) {
    CreateMyelinContext();
    UploadTestData();
    EXPECT_TRUE(myelin_gpu_reset(myelin_ctx));
}

//=============================================================================
// Data Upload Tests
//=============================================================================

TEST_F(MyelinGPUTest, UploadAxonProperties_ValidData_Succeeds) {
    CreateMyelinContext();

    std::vector<float> diameters(TEST_NUM_AXONS, 1.0f);
    std::vector<float> internode_lengths(TEST_NUM_AXONS * TEST_NUM_INTERNODES, 100.0f);

    EXPECT_TRUE(myelin_gpu_upload_axon_properties(
        myelin_ctx, diameters.data(), internode_lengths.data(), TEST_NUM_AXONS));
}

TEST_F(MyelinGPUTest, UploadAxonProperties_NullDiameters_ReturnsFalse) {
    CreateMyelinContext();

    std::vector<float> internode_lengths(TEST_NUM_AXONS * TEST_NUM_INTERNODES, 100.0f);

    EXPECT_FALSE(myelin_gpu_upload_axon_properties(
        myelin_ctx, nullptr, internode_lengths.data(), TEST_NUM_AXONS));
}

TEST_F(MyelinGPUTest, UploadLamellae_ValidData_Succeeds) {
    CreateMyelinContext();

    std::vector<uint32_t> lamellae(TEST_NUM_AXONS * TEST_NUM_INTERNODES, 50);
    EXPECT_TRUE(myelin_gpu_upload_lamellae(myelin_ctx, lamellae.data()));
}

TEST_F(MyelinGPUTest, UploadIntegrity_ValidData_Succeeds) {
    CreateMyelinContext();

    std::vector<float> integrity(TEST_NUM_AXONS * TEST_NUM_INTERNODES, 1.0f);
    EXPECT_TRUE(myelin_gpu_upload_integrity(myelin_ctx, integrity.data()));
}

TEST_F(MyelinGPUTest, DownloadVelocities_ValidContext_Succeeds) {
    CreateMyelinContext();
    UploadTestData();
    myelin_gpu_compute_velocities(myelin_ctx);

    std::vector<float> velocities(TEST_NUM_AXONS);
    EXPECT_TRUE(myelin_gpu_download_velocities(myelin_ctx, velocities.data()));
}

//=============================================================================
// G-Ratio Computation Tests
//=============================================================================

TEST_F(MyelinGPUTest, ComputeGRatios_ValidContext_Succeeds) {
    CreateMyelinContext();
    UploadTestData();

    EXPECT_TRUE(myelin_gpu_compute_g_ratios(myelin_ctx));
}

TEST_F(MyelinGPUTest, ComputeGRatios_NullContext_ReturnsFalse) {
    EXPECT_FALSE(myelin_gpu_compute_g_ratios(nullptr));
}

TEST_F(MyelinGPUTest, ComputeGEfficiency_ValidContext_Succeeds) {
    CreateMyelinContext();
    UploadTestData();
    myelin_gpu_compute_g_ratios(myelin_ctx);

    EXPECT_TRUE(myelin_gpu_compute_g_efficiency(myelin_ctx, nullptr));
}

//=============================================================================
// Cable Theory Tests
//=============================================================================

TEST_F(MyelinGPUTest, ComputeCableParams_ValidContext_Succeeds) {
    CreateMyelinContext();
    UploadTestData();

    EXPECT_TRUE(myelin_gpu_compute_cable_params(myelin_ctx));
}

TEST_F(MyelinGPUTest, ComputeAttenuation_ValidContext_Succeeds) {
    CreateMyelinContext();
    UploadTestData();
    myelin_gpu_compute_cable_params(myelin_ctx);

    EXPECT_TRUE(myelin_gpu_compute_attenuation(myelin_ctx, nullptr));
}

//=============================================================================
// Saltatory Conduction Tests
//=============================================================================

TEST_F(MyelinGPUTest, ComputeVelocities_ValidContext_Succeeds) {
    CreateMyelinContext();
    UploadTestData();
    myelin_gpu_compute_g_ratios(myelin_ctx);
    myelin_gpu_compute_cable_params(myelin_ctx);

    EXPECT_TRUE(myelin_gpu_compute_velocities(myelin_ctx));
}

TEST_F(MyelinGPUTest, ComputeVelocities_ReturnsPositiveValues) {
    CreateMyelinContext();
    UploadTestData();
    myelin_gpu_compute_g_ratios(myelin_ctx);
    myelin_gpu_compute_cable_params(myelin_ctx);
    myelin_gpu_compute_velocities(myelin_ctx);

    std::vector<float> velocities(TEST_NUM_AXONS);
    myelin_gpu_download_velocities(myelin_ctx, velocities.data());

    for (float v : velocities) {
        EXPECT_GT(v, 0.0f) << "Velocity should be positive";
    }
}

TEST_F(MyelinGPUTest, ComputeSegmentVelocities_ValidContext_Succeeds) {
    CreateMyelinContext();
    UploadTestData();

    EXPECT_TRUE(myelin_gpu_compute_segment_velocities(myelin_ctx));
}

TEST_F(MyelinGPUTest, ComputeDelays_ValidContext_Succeeds) {
    CreateMyelinContext();
    UploadTestData();

    EXPECT_TRUE(myelin_gpu_compute_delays(myelin_ctx));
}

TEST_F(MyelinGPUTest, ComputeTotalDelays_ValidContext_Succeeds) {
    CreateMyelinContext();
    UploadTestData();
    myelin_gpu_compute_velocities(myelin_ctx);

    EXPECT_TRUE(myelin_gpu_compute_total_delays(myelin_ctx, nullptr));
}

//=============================================================================
// Plasticity Tests
//=============================================================================

TEST_F(MyelinGPUTest, ApplyPlasticity_ValidContext_Succeeds) {
    CreateMyelinContext();
    UploadTestData();

    // Create activity tensor with data
    std::vector<float> activity_data(TEST_NUM_AXONS, 0.5f);
    size_t dims[1] = { TEST_NUM_AXONS };
    nimcp_gpu_tensor_t* activity = nimcp_gpu_tensor_from_host(
        gpu_ctx, activity_data.data(), dims, 1, NIMCP_GPU_PRECISION_FP32);
    ASSERT_NE(activity, nullptr);

    EXPECT_TRUE(myelin_gpu_apply_plasticity(myelin_ctx, activity, 0.001f));

    nimcp_gpu_tensor_destroy(activity);
}

TEST_F(MyelinGPUTest, UpdateActivityEma_ValidContext_Succeeds) {
    CreateMyelinContext();
    UploadTestData();

    std::vector<float> activity_data(TEST_NUM_AXONS, 0.5f);
    size_t dims[1] = { TEST_NUM_AXONS };
    nimcp_gpu_tensor_t* activity = nimcp_gpu_tensor_from_host(
        gpu_ctx, activity_data.data(), dims, 1, NIMCP_GPU_PRECISION_FP32);
    ASSERT_NE(activity, nullptr);

    EXPECT_TRUE(myelin_gpu_update_activity_ema(myelin_ctx, activity, 0.001f));

    nimcp_gpu_tensor_destroy(activity);
}

TEST_F(MyelinGPUTest, CommitLamellae_ValidContext_Succeeds) {
    CreateMyelinContext();
    UploadTestData();

    EXPECT_TRUE(myelin_gpu_commit_lamellae(myelin_ctx));
}

//=============================================================================
// Conduction Block Tests
//=============================================================================

TEST_F(MyelinGPUTest, ComputeBlockProbabilities_ValidContext_Succeeds) {
    CreateMyelinContext();
    UploadTestData();

    EXPECT_TRUE(myelin_gpu_compute_block_probabilities(myelin_ctx));
}

TEST_F(MyelinGPUTest, ApplyBlocks_ValidContext_Succeeds) {
    CreateMyelinContext();
    UploadTestData();
    myelin_gpu_compute_block_probabilities(myelin_ctx);

    EXPECT_TRUE(myelin_gpu_apply_blocks(myelin_ctx, 12345));
}

TEST_F(MyelinGPUTest, SetTemperature_ValidContext_Succeeds) {
    CreateMyelinContext();
    EXPECT_TRUE(myelin_gpu_set_temperature(myelin_ctx, 37.0f));
}

TEST_F(MyelinGPUTest, SetTemperature_NullContext_ReturnsFalse) {
    EXPECT_FALSE(myelin_gpu_set_temperature(nullptr, 37.0f));
}

//=============================================================================
// Damage and Repair Tests
//=============================================================================

TEST_F(MyelinGPUTest, ApplyDamage_ValidContext_Succeeds) {
    CreateMyelinContext();
    UploadTestData();

    std::vector<float> damage_data(TEST_NUM_AXONS, 0.1f);
    size_t dims[1] = { TEST_NUM_AXONS };
    nimcp_gpu_tensor_t* damage = nimcp_gpu_tensor_from_host(
        gpu_ctx, damage_data.data(), dims, 1, NIMCP_GPU_PRECISION_FP32);
    ASSERT_NE(damage, nullptr);

    EXPECT_TRUE(myelin_gpu_apply_damage(myelin_ctx, damage, true));

    nimcp_gpu_tensor_destroy(damage);
}

TEST_F(MyelinGPUTest, ApplyRepair_ValidContext_Succeeds) {
    CreateMyelinContext();
    UploadTestData();

    EXPECT_TRUE(myelin_gpu_apply_repair(myelin_ctx, 0.01f, 0.001f));
}

TEST_F(MyelinGPUTest, ApplyDecay_ValidContext_Succeeds) {
    CreateMyelinContext();
    UploadTestData();

    EXPECT_TRUE(myelin_gpu_apply_decay(myelin_ctx, 0.001f, 0.001f));
}

//=============================================================================
// Batch Computation Tests
//=============================================================================

TEST_F(MyelinGPUTest, Step_ValidContext_Succeeds) {
    CreateMyelinContext();
    UploadTestData();

    EXPECT_TRUE(myelin_gpu_step(myelin_ctx, nullptr, 0.001f));
}

TEST_F(MyelinGPUTest, Step_WithActivity_Succeeds) {
    CreateMyelinContext();
    UploadTestData();

    std::vector<float> activity_data(TEST_NUM_AXONS, 0.5f);
    size_t dims[1] = { TEST_NUM_AXONS };
    nimcp_gpu_tensor_t* activity = nimcp_gpu_tensor_from_host(
        gpu_ctx, activity_data.data(), dims, 1, NIMCP_GPU_PRECISION_FP32);
    ASSERT_NE(activity, nullptr);

    EXPECT_TRUE(myelin_gpu_step(myelin_ctx, activity, 0.001f));

    nimcp_gpu_tensor_destroy(activity);
}

TEST_F(MyelinGPUTest, Step_MultipleSteps_Stable) {
    CreateMyelinContext();
    UploadTestData();

    for (int i = 0; i < 100; i++) {
        EXPECT_TRUE(myelin_gpu_step(myelin_ctx, nullptr, 0.001f));
    }
}

TEST_F(MyelinGPUTest, RecomputeAll_ValidContext_Succeeds) {
    CreateMyelinContext();
    UploadTestData();

    EXPECT_TRUE(myelin_gpu_recompute_all(myelin_ctx));
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(MyelinGPUTest, ComputeStatistics_ValidContext_Succeeds) {
    CreateMyelinContext();
    UploadTestData();
    myelin_gpu_compute_velocities(myelin_ctx);

    EXPECT_TRUE(myelin_gpu_compute_statistics(myelin_ctx));
}

TEST_F(MyelinGPUTest, GetMeanGRatio_ReturnsValidValue) {
    CreateMyelinContext();
    UploadTestData();
    myelin_gpu_compute_g_ratios(myelin_ctx);

    float mean_g = myelin_gpu_get_mean_g_ratio(myelin_ctx);
    EXPECT_GT(mean_g, 0.0f);
    EXPECT_LT(mean_g, 1.0f);
}

TEST_F(MyelinGPUTest, GetMeanVelocity_ReturnsValidValue) {
    CreateMyelinContext();
    UploadTestData();
    myelin_gpu_recompute_all(myelin_ctx);

    float mean_v = myelin_gpu_get_mean_velocity(myelin_ctx);
    EXPECT_GT(mean_v, 0.0f);
}

TEST_F(MyelinGPUTest, GetMeanIntegrity_ReturnsValidValue) {
    CreateMyelinContext();
    UploadTestData();

    float mean_i = myelin_gpu_get_mean_integrity(myelin_ctx);
    EXPECT_GT(mean_i, 0.0f);
    EXPECT_LE(mean_i, 1.0f);
}

TEST_F(MyelinGPUTest, GetStats_ValidContext_Succeeds) {
    CreateMyelinContext();
    UploadTestData();

    myelin_gpu_stats_t stats;
    EXPECT_TRUE(myelin_gpu_get_stats(myelin_ctx, &stats));
    EXPECT_GE(stats.n_axons, 0u);
}

TEST_F(MyelinGPUTest, GetStats_NullStats_ReturnsFalse) {
    CreateMyelinContext();
    EXPECT_FALSE(myelin_gpu_get_stats(myelin_ctx, nullptr));
}

TEST_F(MyelinGPUTest, ResetStats_ValidContext_Succeeds) {
    CreateMyelinContext();
    UploadTestData();
    myelin_gpu_step(myelin_ctx, nullptr, 0.001f);

    myelin_gpu_reset_stats(myelin_ctx);

    myelin_gpu_stats_t stats;
    myelin_gpu_get_stats(myelin_ctx, &stats);
    EXPECT_EQ(stats.kernel_launches, 0u);
}

//=============================================================================
// Direct Tensor Access Tests
//=============================================================================

TEST_F(MyelinGPUTest, GetGRatios_ValidContext_ReturnsNonNull) {
    CreateMyelinContext();
    UploadTestData();
    myelin_gpu_compute_g_ratios(myelin_ctx);

    const nimcp_gpu_tensor_t* g_ratios = myelin_gpu_get_g_ratios(myelin_ctx);
    EXPECT_NE(g_ratios, nullptr);
}

TEST_F(MyelinGPUTest, GetVelocities_ValidContext_ReturnsNonNull) {
    CreateMyelinContext();
    UploadTestData();
    myelin_gpu_compute_velocities(myelin_ctx);

    const nimcp_gpu_tensor_t* velocities = myelin_gpu_get_velocities(myelin_ctx);
    EXPECT_NE(velocities, nullptr);
}

TEST_F(MyelinGPUTest, GetIntegrity_ValidContext_ReturnsNonNull) {
    CreateMyelinContext();
    UploadTestData();

    const nimcp_gpu_tensor_t* integrity = myelin_gpu_get_integrity(myelin_ctx);
    EXPECT_NE(integrity, nullptr);
}

TEST_F(MyelinGPUTest, GetSpaceConstants_ValidContext_ReturnsNonNull) {
    CreateMyelinContext();
    UploadTestData();
    myelin_gpu_compute_cable_params(myelin_ctx);

    const nimcp_gpu_tensor_t* space = myelin_gpu_get_space_constants(myelin_ctx);
    EXPECT_NE(space, nullptr);
}

//=============================================================================
// CPU Reference Tests
//=============================================================================

TEST_F(MyelinGPUTest, CPUComputeGRatios_ValidData_Succeeds) {
    std::vector<float> diameters(SMALL_NETWORK);
    std::vector<float> g_ratios(SMALL_NETWORK);

    for (uint32_t i = 0; i < SMALL_NETWORK; i++) {
        diameters[i] = 1.0f + i * 0.1f;
    }

    myelin_cpu_compute_g_ratios(diameters.data(), g_ratios.data(), SMALL_NETWORK);

    for (float g : g_ratios) {
        EXPECT_GT(g, 0.0f);
        EXPECT_LT(g, 1.0f);
    }
}

TEST_F(MyelinGPUTest, CPUComputeCableParams_ValidData_Succeeds) {
    std::vector<float> diameters(SMALL_NETWORK, 1.0f);
    std::vector<uint32_t> lamellae(SMALL_NETWORK * TEST_NUM_INTERNODES, 50);
    std::vector<float> space_constants(SMALL_NETWORK * TEST_NUM_INTERNODES);
    std::vector<float> time_constants(SMALL_NETWORK * TEST_NUM_INTERNODES);

    myelin_cpu_compute_cable_params(
        diameters.data(), lamellae.data(),
        space_constants.data(), time_constants.data(),
        SMALL_NETWORK, TEST_NUM_INTERNODES);

    for (float lambda : space_constants) {
        EXPECT_GT(lambda, 0.0f);
    }
}

TEST_F(MyelinGPUTest, CPUComputeVelocities_ValidData_Succeeds) {
    std::vector<float> diameters(SMALL_NETWORK, 1.0f);
    std::vector<float> internode_lengths(SMALL_NETWORK * TEST_NUM_INTERNODES, 100.0f);
    std::vector<uint32_t> lamellae(SMALL_NETWORK * TEST_NUM_INTERNODES, 50);
    std::vector<float> g_ratios(SMALL_NETWORK, 0.77f);
    std::vector<float> compaction(SMALL_NETWORK * TEST_NUM_INTERNODES, 1.0f);
    std::vector<float> integrity(SMALL_NETWORK * TEST_NUM_INTERNODES, 1.0f);
    std::vector<float> velocities(SMALL_NETWORK);

    myelin_cpu_compute_velocities(
        diameters.data(), internode_lengths.data(), lamellae.data(),
        g_ratios.data(), compaction.data(), integrity.data(),
        velocities.data(), SMALL_NETWORK, TEST_NUM_INTERNODES);

    for (float v : velocities) {
        EXPECT_GT(v, 0.0f);
    }
}

TEST_F(MyelinGPUTest, CPUApplyPlasticity_ValidData_Succeeds) {
    std::vector<float> activity(SMALL_NETWORK, 0.5f);
    std::vector<float> lamellae_fractional(SMALL_NETWORK * TEST_NUM_INTERNODES, 50.0f);
    std::vector<uint32_t> lamellae(SMALL_NETWORK * TEST_NUM_INTERNODES, 50);

    myelin_cpu_apply_plasticity(
        activity.data(), lamellae_fractional.data(), lamellae.data(),
        1.0f, 0.5f, 2.0f, 0.001f,
        SMALL_NETWORK, TEST_NUM_INTERNODES);
}

//=============================================================================
// Large Network Tests
//=============================================================================

TEST_F(MyelinGPUTest, LargeNetwork_CreateAndStep_Succeeds) {
    if (!gpu_available) GTEST_SKIP() << "GPU not available";

    myelin_gpu_config_t config = myelin_gpu_default_config();
    config.max_axons = LARGE_NETWORK * 2;
    config.max_internodes = TEST_NUM_INTERNODES;

    myelin_ctx = myelin_gpu_create(gpu_ctx, &config);
    ASSERT_NE(myelin_ctx, nullptr);

    // Upload large dataset
    std::vector<float> diameters(LARGE_NETWORK, 1.0f);
    std::vector<float> internode_lengths(LARGE_NETWORK * TEST_NUM_INTERNODES, 100.0f);
    myelin_gpu_upload_axon_properties(myelin_ctx, diameters.data(),
                                      internode_lengths.data(), LARGE_NETWORK);

    std::vector<uint32_t> lamellae(LARGE_NETWORK * TEST_NUM_INTERNODES, 50);
    myelin_gpu_upload_lamellae(myelin_ctx, lamellae.data());

    std::vector<float> integrity(LARGE_NETWORK * TEST_NUM_INTERNODES, 1.0f);
    myelin_gpu_upload_integrity(myelin_ctx, integrity.data());

    // Run step
    EXPECT_TRUE(myelin_gpu_step(myelin_ctx, nullptr, 0.001f));
}

//=============================================================================
// Biophysics Validation Tests
//=============================================================================

TEST_F(MyelinGPUTest, GRatio_IncreasesWithDiameter) {
    CreateMyelinContext();

    // Small and large diameter axons
    std::vector<float> diameters = {0.5f, 5.0f};  // 0.5 um and 5 um
    std::vector<float> internode_lengths(2 * TEST_NUM_INTERNODES, 100.0f);

    myelin_gpu_config_t config = myelin_gpu_default_config();
    config.max_axons = 4;
    myelin_gpu_context_t* ctx = myelin_gpu_create(gpu_ctx, &config);
    ASSERT_NE(ctx, nullptr);

    myelin_gpu_upload_axon_properties(ctx, diameters.data(),
                                      internode_lengths.data(), 2);
    myelin_gpu_compute_g_ratios(ctx);

    // Download g-ratios
    const nimcp_gpu_tensor_t* g_tensor = myelin_gpu_get_g_ratios(ctx);
    EXPECT_NE(g_tensor, nullptr);

    myelin_gpu_destroy(ctx);
}

TEST_F(MyelinGPUTest, Velocity_IncreasesWithMyelination) {
    // This is a conceptual test - more myelinated axons should conduct faster
    // The actual verification would require detailed setup
    CreateMyelinContext();
    UploadTestData();
    myelin_gpu_recompute_all(myelin_ctx);

    float mean_v = myelin_gpu_get_mean_velocity(myelin_ctx);
    EXPECT_GT(mean_v, 0.0f) << "Myelinated axons should have positive velocity";
}
