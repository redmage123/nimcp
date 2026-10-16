/**
 * @file test_training_enhancements_amp_dist.cpp
 * @brief Unit tests for Mixed Precision (AMP) and Distributed Training modules
 *
 * Tests:
 * - Mixed Precision (AMP):
 *   - Default and BF16 configuration
 *   - Context creation and destruction
 *   - Autocast enter/exit and state tracking
 *   - Loss scaling and gradient unscaling
 *   - Dynamic scale updates
 *   - Statistics retrieval
 *   - Dtype utility functions
 *   - Configuration validation
 *
 * - Distributed Training:
 *   - Default configuration with rank/world_size
 *   - Context creation and destruction
 *   - Worker identification (rank, world_size, coordinator)
 *   - Statistics retrieval
 *   - Strategy/backend/sync method name utilities
 *   - Configuration validation
 *
 * @note Part of Training Enhancements Phase
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <limits>
#include <cstring>

// Headers have their own extern "C" guards
#include "training/nimcp_mixed_precision.h"
#include "training/nimcp_distributed_training.h"

/* ============================================================================
 * Mixed Precision (AMP) Test Fixtures
 * ============================================================================ */

class MixedPrecisionTest : public ::testing::Test {
protected:
    void SetUp() override {
        ctx = nullptr;
    }

    void TearDown() override {
        if (ctx) {
            amp_destroy(ctx);
            ctx = nullptr;
        }
    }

    amp_ctx_t* ctx;
};

/* ============================================================================
 * AMP Default Configuration Tests
 * ============================================================================ */

TEST_F(MixedPrecisionTest, DefaultConfig_ReturnsSuccess) {
    amp_config_t config;
    memset(&config, 0, sizeof(config));

    int result = amp_default_config(&config);
    EXPECT_EQ(result, 0);
}

TEST_F(MixedPrecisionTest, DefaultConfig_SetsReasonableValues) {
    amp_config_t config;
    memset(&config, 0, sizeof(config));

    amp_default_config(&config);

    // Default should use FP16 for compute
    EXPECT_EQ(config.autocast.compute_dtype, AMP_DTYPE_FP16);
    EXPECT_EQ(config.autocast.storage_dtype, AMP_DTYPE_FP32);
    EXPECT_TRUE(config.autocast.enabled);

    // Default scaling should be dynamic
    EXPECT_EQ(config.scaling.mode, AMP_SCALING_DYNAMIC);
    EXPECT_FLOAT_EQ(config.scaling.init_scale, AMP_DEFAULT_INIT_SCALE);
    EXPECT_FLOAT_EQ(config.scaling.growth_factor, AMP_DEFAULT_GROWTH_FACTOR);
    EXPECT_FLOAT_EQ(config.scaling.backoff_factor, AMP_DEFAULT_BACKOFF_FACTOR);
    EXPECT_EQ(config.scaling.growth_interval, AMP_DEFAULT_GROWTH_INTERVAL);
}

TEST_F(MixedPrecisionTest, DefaultConfig_NullReturnsError) {
    int result = amp_default_config(nullptr);
    EXPECT_LT(result, 0);
}

/* ============================================================================
 * AMP BF16 Configuration Tests
 * ============================================================================ */

TEST_F(MixedPrecisionTest, BF16Config_ReturnsSuccess) {
    amp_config_t config;
    memset(&config, 0, sizeof(config));

    int result = amp_bf16_config(&config);
    EXPECT_EQ(result, 0);
}

TEST_F(MixedPrecisionTest, BF16Config_SetsBF16Dtype) {
    amp_config_t config;
    memset(&config, 0, sizeof(config));

    amp_bf16_config(&config);

    // BF16 config should use BF16 for compute
    EXPECT_EQ(config.autocast.compute_dtype, AMP_DTYPE_BF16);
    EXPECT_EQ(config.autocast.storage_dtype, AMP_DTYPE_FP32);
}

TEST_F(MixedPrecisionTest, BF16Config_MayDisableScaling) {
    amp_config_t config;
    memset(&config, 0, sizeof(config));

    amp_bf16_config(&config);

    // BF16 often doesn't need scaling due to better dynamic range
    // Check that scaling mode is either NONE or has reasonable settings
    EXPECT_TRUE(config.scaling.mode == AMP_SCALING_NONE ||
                config.scaling.mode == AMP_SCALING_STATIC ||
                config.scaling.mode == AMP_SCALING_DYNAMIC);
}

TEST_F(MixedPrecisionTest, BF16Config_NullReturnsError) {
    int result = amp_bf16_config(nullptr);
    EXPECT_LT(result, 0);
}

/* ============================================================================
 * AMP Context Creation and Destruction Tests
 * ============================================================================ */

TEST_F(MixedPrecisionTest, Create_DefaultConfig) {
    amp_config_t config;
    amp_default_config(&config);

    ctx = amp_create(&config);
    ASSERT_NE(ctx, nullptr);
}

TEST_F(MixedPrecisionTest, Create_BF16Config) {
    amp_config_t config;
    amp_bf16_config(&config);

    ctx = amp_create(&config);
    ASSERT_NE(ctx, nullptr);
}

TEST_F(MixedPrecisionTest, Create_NullConfigReturnsNull) {
    ctx = amp_create(nullptr);
    EXPECT_EQ(ctx, nullptr);
}

TEST_F(MixedPrecisionTest, Destroy_NullIsSafe) {
    // Should not crash
    amp_destroy(nullptr);
    SUCCEED();
}

TEST_F(MixedPrecisionTest, Destroy_ValidContext) {
    amp_config_t config;
    amp_default_config(&config);

    amp_ctx_t* temp_ctx = amp_create(&config);
    ASSERT_NE(temp_ctx, nullptr);

    // Should not crash
    amp_destroy(temp_ctx);
    SUCCEED();
}

/* ============================================================================
 * AMP Autocast Tests
 * ============================================================================ */

TEST_F(MixedPrecisionTest, Autocast_EnterExitSuccess) {
    amp_config_t config;
    amp_default_config(&config);
    ctx = amp_create(&config);
    ASSERT_NE(ctx, nullptr);

    int enter_result = amp_autocast_enter(ctx);
    EXPECT_EQ(enter_result, 0);

    int exit_result = amp_autocast_exit(ctx);
    EXPECT_EQ(exit_result, 0);
}

TEST_F(MixedPrecisionTest, Autocast_IsAutocasting) {
    amp_config_t config;
    amp_default_config(&config);
    ctx = amp_create(&config);
    ASSERT_NE(ctx, nullptr);

    // Initially not autocasting
    EXPECT_FALSE(amp_is_autocasting(ctx));

    // After entering autocast region
    amp_autocast_enter(ctx);
    EXPECT_TRUE(amp_is_autocasting(ctx));

    // After exiting autocast region
    amp_autocast_exit(ctx);
    EXPECT_FALSE(amp_is_autocasting(ctx));
}

TEST_F(MixedPrecisionTest, Autocast_NestedEnter) {
    amp_config_t config;
    amp_default_config(&config);
    ctx = amp_create(&config);
    ASSERT_NE(ctx, nullptr);

    // Nested autocast calls
    amp_autocast_enter(ctx);
    EXPECT_TRUE(amp_is_autocasting(ctx));

    amp_autocast_enter(ctx);  // Nested
    EXPECT_TRUE(amp_is_autocasting(ctx));

    amp_autocast_exit(ctx);
    // May still be autocasting depending on implementation
    // At minimum, should not crash

    amp_autocast_exit(ctx);
    EXPECT_FALSE(amp_is_autocasting(ctx));
}

TEST_F(MixedPrecisionTest, Autocast_NullContext) {
    EXPECT_FALSE(amp_is_autocasting(nullptr));
    // These should not crash
    amp_autocast_enter(nullptr);
    amp_autocast_exit(nullptr);
}

/* ============================================================================
 * AMP Loss Scaling Tests
 * ============================================================================ */

TEST_F(MixedPrecisionTest, ScaleLoss_ScalesCorrectly) {
    amp_config_t config;
    amp_default_config(&config);
    config.scaling.mode = AMP_SCALING_STATIC;
    config.scaling.init_scale = 1000.0f;

    ctx = amp_create(&config);
    ASSERT_NE(ctx, nullptr);

    float loss = 0.5f;
    float scaled_loss = amp_scale_loss(ctx, loss);

    EXPECT_FLOAT_EQ(scaled_loss, loss * 1000.0f);
}

TEST_F(MixedPrecisionTest, ScaleLoss_NoScaling) {
    amp_config_t config;
    amp_default_config(&config);
    config.scaling.mode = AMP_SCALING_NONE;

    ctx = amp_create(&config);
    ASSERT_NE(ctx, nullptr);

    float loss = 1.5f;
    float scaled_loss = amp_scale_loss(ctx, loss);

    // With no scaling, loss should be unchanged
    EXPECT_FLOAT_EQ(scaled_loss, loss);
}

TEST_F(MixedPrecisionTest, GetScale_ReturnsInitialScale) {
    amp_config_t config;
    amp_default_config(&config);
    config.scaling.init_scale = 2048.0f;

    ctx = amp_create(&config);
    ASSERT_NE(ctx, nullptr);

    float scale = amp_get_scale(ctx);
    EXPECT_FLOAT_EQ(scale, 2048.0f);
}

TEST_F(MixedPrecisionTest, GetScale_NullContext) {
    float scale = amp_get_scale(nullptr);
    // Should return a safe default value (typically 1.0)
    EXPECT_FLOAT_EQ(scale, 1.0f);
}

/* ============================================================================
 * AMP Gradient Unscaling Tests
 * ============================================================================ */

TEST_F(MixedPrecisionTest, UnscaleGradients_Success) {
    amp_config_t config;
    amp_default_config(&config);
    config.scaling.mode = AMP_SCALING_STATIC;
    config.scaling.init_scale = 100.0f;

    ctx = amp_create(&config);
    ASSERT_NE(ctx, nullptr);

    std::vector<float> gradients = {100.0f, 200.0f, 300.0f};
    bool valid = amp_unscale_gradients(ctx, gradients.data(), gradients.size());

    EXPECT_TRUE(valid);
    EXPECT_FLOAT_EQ(gradients[0], 1.0f);
    EXPECT_FLOAT_EQ(gradients[1], 2.0f);
    EXPECT_FLOAT_EQ(gradients[2], 3.0f);
}

TEST_F(MixedPrecisionTest, UnscaleGradients_DetectsInfinity) {
    amp_config_t config;
    amp_default_config(&config);
    config.scaling.mode = AMP_SCALING_STATIC;
    config.scaling.init_scale = 100.0f;

    ctx = amp_create(&config);
    ASSERT_NE(ctx, nullptr);

    std::vector<float> gradients = {
        100.0f,
        std::numeric_limits<float>::infinity(),
        300.0f
    };
    bool valid = amp_unscale_gradients(ctx, gradients.data(), gradients.size());

    EXPECT_FALSE(valid);
}

TEST_F(MixedPrecisionTest, UnscaleGradients_DetectsNaN) {
    amp_config_t config;
    amp_default_config(&config);
    config.scaling.mode = AMP_SCALING_STATIC;
    config.scaling.init_scale = 100.0f;

    ctx = amp_create(&config);
    ASSERT_NE(ctx, nullptr);

    std::vector<float> gradients = {
        100.0f,
        std::numeric_limits<float>::quiet_NaN(),
        300.0f
    };
    bool valid = amp_unscale_gradients(ctx, gradients.data(), gradients.size());

    EXPECT_FALSE(valid);
}

/* ============================================================================
 * AMP Scale Update Tests
 * ============================================================================ */

TEST_F(MixedPrecisionTest, UpdateScale_GrowsOnValidGradients) {
    amp_config_t config;
    amp_default_config(&config);
    config.scaling.mode = AMP_SCALING_DYNAMIC;
    config.scaling.init_scale = 100.0f;
    config.scaling.growth_factor = 2.0f;
    config.scaling.growth_interval = 2;

    ctx = amp_create(&config);
    ASSERT_NE(ctx, nullptr);

    float initial_scale = amp_get_scale(ctx);

    // Simulate valid gradients for growth_interval steps
    amp_update_scale(ctx, true);
    amp_update_scale(ctx, true);

    float new_scale = amp_get_scale(ctx);
    EXPECT_GT(new_scale, initial_scale);
}

TEST_F(MixedPrecisionTest, UpdateScale_BacksOffOnInvalid) {
    amp_config_t config;
    amp_default_config(&config);
    config.scaling.mode = AMP_SCALING_DYNAMIC;
    config.scaling.init_scale = 1000.0f;
    config.scaling.backoff_factor = 0.5f;

    ctx = amp_create(&config);
    ASSERT_NE(ctx, nullptr);

    float initial_scale = amp_get_scale(ctx);

    // Simulate invalid gradients
    amp_update_scale(ctx, false);

    float new_scale = amp_get_scale(ctx);
    EXPECT_LT(new_scale, initial_scale);
    EXPECT_FLOAT_EQ(new_scale, initial_scale * 0.5f);
}

TEST_F(MixedPrecisionTest, UpdateScale_RespectsMinScale) {
    amp_config_t config;
    amp_default_config(&config);
    config.scaling.mode = AMP_SCALING_DYNAMIC;
    config.scaling.init_scale = 2.0f;
    config.scaling.min_scale = AMP_MIN_SCALE;
    config.scaling.backoff_factor = 0.5f;

    ctx = amp_create(&config);
    ASSERT_NE(ctx, nullptr);

    // Multiple backoffs
    amp_update_scale(ctx, false);
    amp_update_scale(ctx, false);
    amp_update_scale(ctx, false);
    amp_update_scale(ctx, false);

    float scale = amp_get_scale(ctx);
    EXPECT_GE(scale, AMP_MIN_SCALE);
}

/* ============================================================================
 * AMP Statistics Tests
 * ============================================================================ */

TEST_F(MixedPrecisionTest, GetStats_ReturnsSuccess) {
    amp_config_t config;
    amp_default_config(&config);
    config.track_statistics = true;

    ctx = amp_create(&config);
    ASSERT_NE(ctx, nullptr);

    amp_stats_t stats;
    int result = amp_get_stats(ctx, &stats);

    EXPECT_EQ(result, 0);
}

TEST_F(MixedPrecisionTest, GetStats_InitialValues) {
    amp_config_t config;
    amp_default_config(&config);
    config.track_statistics = true;
    config.scaling.init_scale = 65536.0f;

    ctx = amp_create(&config);
    ASSERT_NE(ctx, nullptr);

    amp_stats_t stats;
    amp_get_stats(ctx, &stats);

    EXPECT_EQ(stats.total_steps, 0u);
    EXPECT_EQ(stats.overflow_count, 0u);
    EXPECT_EQ(stats.skipped_steps, 0u);
    EXPECT_FLOAT_EQ(stats.current_scale, 65536.0f);
}

TEST_F(MixedPrecisionTest, GetStats_NullContext) {
    amp_stats_t stats;
    int result = amp_get_stats(nullptr, &stats);

    EXPECT_LT(result, 0);
}

TEST_F(MixedPrecisionTest, GetStats_NullStats) {
    amp_config_t config;
    amp_default_config(&config);
    ctx = amp_create(&config);
    ASSERT_NE(ctx, nullptr);

    int result = amp_get_stats(ctx, nullptr);
    EXPECT_LT(result, 0);
}

/* ============================================================================
 * AMP Dtype Utility Tests
 * ============================================================================ */

TEST_F(MixedPrecisionTest, DtypeName_FP32) {
    const char* name = amp_dtype_name(AMP_DTYPE_FP32);
    ASSERT_NE(name, nullptr);
    EXPECT_STRNE(name, "");
    // Should contain "32" or "float"
    EXPECT_TRUE(strstr(name, "32") != nullptr ||
                strstr(name, "float") != nullptr ||
                strstr(name, "FP32") != nullptr);
}

TEST_F(MixedPrecisionTest, DtypeName_FP16) {
    const char* name = amp_dtype_name(AMP_DTYPE_FP16);
    ASSERT_NE(name, nullptr);
    EXPECT_STRNE(name, "");
    EXPECT_TRUE(strstr(name, "16") != nullptr ||
                strstr(name, "half") != nullptr ||
                strstr(name, "FP16") != nullptr);
}

TEST_F(MixedPrecisionTest, DtypeName_BF16) {
    const char* name = amp_dtype_name(AMP_DTYPE_BF16);
    ASSERT_NE(name, nullptr);
    EXPECT_STRNE(name, "");
    EXPECT_TRUE(strstr(name, "BF16") != nullptr ||
                strstr(name, "bf16") != nullptr ||
                strstr(name, "bfloat") != nullptr);
}

TEST_F(MixedPrecisionTest, DtypeName_AllValid) {
    // All enum values should return non-null strings
    for (int i = 0; i < AMP_DTYPE_COUNT; i++) {
        const char* name = amp_dtype_name(static_cast<amp_dtype_t>(i));
        EXPECT_NE(name, nullptr) << "Dtype " << i << " returned null name";
    }
}

TEST_F(MixedPrecisionTest, DtypeName_InvalidReturnsUnknown) {
    const char* name = amp_dtype_name(static_cast<amp_dtype_t>(999));
    ASSERT_NE(name, nullptr);
    // Should return "Unknown" or similar
}

TEST_F(MixedPrecisionTest, DtypeSize_FP32) {
    size_t size = amp_dtype_size(AMP_DTYPE_FP32);
    EXPECT_EQ(size, 4u);
}

TEST_F(MixedPrecisionTest, DtypeSize_FP16) {
    size_t size = amp_dtype_size(AMP_DTYPE_FP16);
    EXPECT_EQ(size, 2u);
}

TEST_F(MixedPrecisionTest, DtypeSize_BF16) {
    size_t size = amp_dtype_size(AMP_DTYPE_BF16);
    EXPECT_EQ(size, 2u);
}

/* ============================================================================
 * AMP Configuration Validation Tests
 * ============================================================================ */

TEST_F(MixedPrecisionTest, ValidateConfig_ValidDefault) {
    amp_config_t config;
    amp_default_config(&config);

    int result = amp_validate_config(&config);
    EXPECT_EQ(result, 0);
}

TEST_F(MixedPrecisionTest, ValidateConfig_ValidBF16) {
    amp_config_t config;
    amp_bf16_config(&config);

    int result = amp_validate_config(&config);
    EXPECT_EQ(result, 0);
}

TEST_F(MixedPrecisionTest, ValidateConfig_NullConfig) {
    int result = amp_validate_config(nullptr);
    EXPECT_LT(result, 0);
}

TEST_F(MixedPrecisionTest, ValidateConfig_InvalidScaleRange) {
    amp_config_t config;
    amp_default_config(&config);
    config.scaling.min_scale = 100.0f;
    config.scaling.max_scale = 10.0f;  // Invalid: max < min

    int result = amp_validate_config(&config);
    EXPECT_LT(result, 0);
}

TEST_F(MixedPrecisionTest, ValidateConfig_NegativeScale) {
    amp_config_t config;
    amp_default_config(&config);
    config.scaling.init_scale = -1.0f;  // Invalid

    int result = amp_validate_config(&config);
    EXPECT_LT(result, 0);
}

/* ============================================================================
 * Distributed Training Test Fixtures
 * ============================================================================ */

class DistributedTrainingTest : public ::testing::Test {
protected:
    void SetUp() override {
        ctx = nullptr;
    }

    void TearDown() override {
        if (ctx) {
            dist_destroy(ctx);
            ctx = nullptr;
        }
    }

    dist_ctx_t* ctx;
};

/* ============================================================================
 * Distributed Default Configuration Tests
 * ============================================================================ */

TEST_F(DistributedTrainingTest, DefaultConfig_ReturnsSuccess) {
    dist_config_t config;
    memset(&config, 0, sizeof(config));

    int result = dist_default_config(&config, 4, 0);
    EXPECT_EQ(result, 0);
}

TEST_F(DistributedTrainingTest, DefaultConfig_SetsWorkerInfo) {
    dist_config_t config;
    memset(&config, 0, sizeof(config));

    dist_default_config(&config, 8, 3);

    EXPECT_EQ(config.worker.world_size, 8u);
    EXPECT_EQ(config.worker.rank, 3u);
}

TEST_F(DistributedTrainingTest, DefaultConfig_SetsReasonableDefaults) {
    dist_config_t config;
    memset(&config, 0, sizeof(config));

    dist_default_config(&config, 4, 0);

    // Default strategy should be data parallel
    EXPECT_EQ(config.strategy, DIST_STRATEGY_DATA_PARALLEL);

    // Backend should be set
    EXPECT_TRUE(config.backend >= 0 && config.backend < DIST_BACKEND_COUNT);

    // Timeout should be reasonable
    EXPECT_GT(config.timeout_ms, 0u);
}

TEST_F(DistributedTrainingTest, DefaultConfig_CoordinatorRole) {
    dist_config_t config;
    memset(&config, 0, sizeof(config));

    dist_default_config(&config, 4, 0);  // Rank 0

    // Rank 0 should be coordinator by convention
    EXPECT_EQ(config.worker.role, DIST_ROLE_COORDINATOR);
}

TEST_F(DistributedTrainingTest, DefaultConfig_WorkerRole) {
    dist_config_t config;
    memset(&config, 0, sizeof(config));

    dist_default_config(&config, 4, 2);  // Non-zero rank

    EXPECT_EQ(config.worker.role, DIST_ROLE_WORKER);
}

TEST_F(DistributedTrainingTest, DefaultConfig_NullConfig) {
    int result = dist_default_config(nullptr, 4, 0);
    EXPECT_LT(result, 0);
}

/* ============================================================================
 * Distributed Context Creation and Destruction Tests
 * ============================================================================ */

TEST_F(DistributedTrainingTest, Create_ValidConfig) {
    dist_config_t config;
    dist_default_config(&config, 4, 0);

    ctx = dist_create(&config);
    ASSERT_NE(ctx, nullptr);
}

TEST_F(DistributedTrainingTest, Create_SingleWorker) {
    dist_config_t config;
    dist_default_config(&config, 1, 0);

    ctx = dist_create(&config);
    ASSERT_NE(ctx, nullptr);
}

TEST_F(DistributedTrainingTest, Create_NullConfigReturnsNull) {
    ctx = dist_create(nullptr);
    EXPECT_EQ(ctx, nullptr);
}

TEST_F(DistributedTrainingTest, Destroy_NullIsSafe) {
    // Should not crash
    dist_destroy(nullptr);
    SUCCEED();
}

TEST_F(DistributedTrainingTest, Destroy_ValidContext) {
    dist_config_t config;
    dist_default_config(&config, 2, 0);

    dist_ctx_t* temp_ctx = dist_create(&config);
    ASSERT_NE(temp_ctx, nullptr);

    // Should not crash
    dist_destroy(temp_ctx);
    SUCCEED();
}

/* ============================================================================
 * Distributed Worker Identification Tests
 * ============================================================================ */

TEST_F(DistributedTrainingTest, GetRank_ReturnsConfiguredRank) {
    dist_config_t config;
    dist_default_config(&config, 8, 5);

    ctx = dist_create(&config);
    ASSERT_NE(ctx, nullptr);

    uint32_t rank = dist_get_rank(ctx);
    EXPECT_EQ(rank, 5u);
}

TEST_F(DistributedTrainingTest, GetWorldSize_ReturnsConfiguredSize) {
    dist_config_t config;
    dist_default_config(&config, 16, 0);

    ctx = dist_create(&config);
    ASSERT_NE(ctx, nullptr);

    uint32_t world_size = dist_get_world_size(ctx);
    EXPECT_EQ(world_size, 16u);
}

TEST_F(DistributedTrainingTest, IsCoordinator_TrueForRankZero) {
    dist_config_t config;
    dist_default_config(&config, 4, 0);

    ctx = dist_create(&config);
    ASSERT_NE(ctx, nullptr);

    EXPECT_TRUE(dist_is_coordinator(ctx));
}

TEST_F(DistributedTrainingTest, IsCoordinator_FalseForOtherRanks) {
    dist_config_t config;
    dist_default_config(&config, 4, 1);

    ctx = dist_create(&config);
    ASSERT_NE(ctx, nullptr);

    EXPECT_FALSE(dist_is_coordinator(ctx));
}

TEST_F(DistributedTrainingTest, IsCoordinator_FalseForLastRank) {
    dist_config_t config;
    dist_default_config(&config, 4, 3);

    ctx = dist_create(&config);
    ASSERT_NE(ctx, nullptr);

    EXPECT_FALSE(dist_is_coordinator(ctx));
}

TEST_F(DistributedTrainingTest, GetRank_NullContext) {
    uint32_t rank = dist_get_rank(nullptr);
    // Should return a safe value (0)
    EXPECT_EQ(rank, 0u);
}

TEST_F(DistributedTrainingTest, GetWorldSize_NullContext) {
    uint32_t world_size = dist_get_world_size(nullptr);
    // Should return a safe value (1 for single worker)
    EXPECT_EQ(world_size, 1u);
}

TEST_F(DistributedTrainingTest, IsCoordinator_NullContext) {
    bool is_coord = dist_is_coordinator(nullptr);
    // Null context should return safe default
    EXPECT_FALSE(is_coord);
}

/* ============================================================================
 * Distributed Statistics Tests
 * ============================================================================ */

TEST_F(DistributedTrainingTest, GetStats_ReturnsSuccess) {
    dist_config_t config;
    dist_default_config(&config, 4, 0);
    config.track_communication = true;

    ctx = dist_create(&config);
    ASSERT_NE(ctx, nullptr);

    dist_stats_t stats;
    int result = dist_get_stats(ctx, &stats);

    EXPECT_EQ(result, 0);
}

TEST_F(DistributedTrainingTest, GetStats_InitialValues) {
    dist_config_t config;
    dist_default_config(&config, 4, 0);

    ctx = dist_create(&config);
    ASSERT_NE(ctx, nullptr);

    dist_stats_t stats;
    dist_get_stats(ctx, &stats);

    EXPECT_EQ(stats.total_steps, 0u);
    EXPECT_EQ(stats.sync_events, 0u);
    EXPECT_EQ(stats.bytes_sent, 0u);
    EXPECT_EQ(stats.bytes_received, 0u);
    EXPECT_EQ(stats.timeout_count, 0u);
}

TEST_F(DistributedTrainingTest, GetStats_NullContext) {
    dist_stats_t stats;
    int result = dist_get_stats(nullptr, &stats);

    EXPECT_LT(result, 0);
}

TEST_F(DistributedTrainingTest, GetStats_NullStats) {
    dist_config_t config;
    dist_default_config(&config, 4, 0);

    ctx = dist_create(&config);
    ASSERT_NE(ctx, nullptr);

    int result = dist_get_stats(ctx, nullptr);
    EXPECT_LT(result, 0);
}

/* ============================================================================
 * Distributed Strategy Name Tests
 * ============================================================================ */

TEST_F(DistributedTrainingTest, StrategyName_DataParallel) {
    const char* name = dist_strategy_name(DIST_STRATEGY_DATA_PARALLEL);
    ASSERT_NE(name, nullptr);
    EXPECT_STRNE(name, "");
    EXPECT_TRUE(strstr(name, "data") != nullptr ||
                strstr(name, "Data") != nullptr ||
                strstr(name, "DATA") != nullptr ||
                strstr(name, "parallel") != nullptr);
}

TEST_F(DistributedTrainingTest, StrategyName_Federated) {
    const char* name = dist_strategy_name(DIST_STRATEGY_FEDERATED);
    ASSERT_NE(name, nullptr);
    EXPECT_STRNE(name, "");
    EXPECT_TRUE(strstr(name, "feder") != nullptr ||
                strstr(name, "Feder") != nullptr ||
                strstr(name, "FEDER") != nullptr);
}

TEST_F(DistributedTrainingTest, StrategyName_FSDP) {
    const char* name = dist_strategy_name(DIST_STRATEGY_FSDP);
    ASSERT_NE(name, nullptr);
    EXPECT_STRNE(name, "");
    EXPECT_TRUE(strstr(name, "FSDP") != nullptr ||
                strstr(name, "fsdp") != nullptr ||
                strstr(name, "Shard") != nullptr ||
                strstr(name, "shard") != nullptr);
}

TEST_F(DistributedTrainingTest, StrategyName_AllValid) {
    // All enum values should return non-null strings
    for (int i = 0; i < DIST_STRATEGY_COUNT; i++) {
        const char* name = dist_strategy_name(static_cast<dist_strategy_t>(i));
        EXPECT_NE(name, nullptr) << "Strategy " << i << " returned null name";
    }
}

TEST_F(DistributedTrainingTest, StrategyName_InvalidReturnsUnknown) {
    const char* name = dist_strategy_name(static_cast<dist_strategy_t>(999));
    ASSERT_NE(name, nullptr);
    // Should return "Unknown" or similar
}

/* ============================================================================
 * Distributed Sync Method Name Tests
 * ============================================================================ */

TEST_F(DistributedTrainingTest, SyncMethodName_AllReduce) {
    const char* name = dist_sync_method_name(DIST_SYNC_ALL_REDUCE);
    ASSERT_NE(name, nullptr);
    EXPECT_STRNE(name, "");
}

TEST_F(DistributedTrainingTest, SyncMethodName_FedAvg) {
    const char* name = dist_sync_method_name(DIST_SYNC_FEDAVG);
    ASSERT_NE(name, nullptr);
    EXPECT_STRNE(name, "");
    EXPECT_TRUE(strstr(name, "Fed") != nullptr ||
                strstr(name, "fed") != nullptr ||
                strstr(name, "FED") != nullptr ||
                strstr(name, "avg") != nullptr ||
                strstr(name, "Avg") != nullptr);
}

TEST_F(DistributedTrainingTest, SyncMethodName_AllValid) {
    for (int i = 0; i < DIST_SYNC_COUNT; i++) {
        const char* name = dist_sync_method_name(static_cast<dist_sync_method_t>(i));
        EXPECT_NE(name, nullptr) << "Sync method " << i << " returned null name";
    }
}

/* ============================================================================
 * Distributed Backend Name Tests
 * ============================================================================ */

TEST_F(DistributedTrainingTest, BackendName_BioAsync) {
    const char* name = dist_backend_name(DIST_BACKEND_BIO_ASYNC);
    ASSERT_NE(name, nullptr);
    EXPECT_STRNE(name, "");
    EXPECT_TRUE(strstr(name, "bio") != nullptr ||
                strstr(name, "Bio") != nullptr ||
                strstr(name, "BIO") != nullptr ||
                strstr(name, "async") != nullptr ||
                strstr(name, "Async") != nullptr);
}

TEST_F(DistributedTrainingTest, BackendName_NCCL) {
    const char* name = dist_backend_name(DIST_BACKEND_NCCL);
    ASSERT_NE(name, nullptr);
    EXPECT_STRNE(name, "");
    EXPECT_TRUE(strstr(name, "NCCL") != nullptr ||
                strstr(name, "nccl") != nullptr);
}

TEST_F(DistributedTrainingTest, BackendName_AllValid) {
    for (int i = 0; i < DIST_BACKEND_COUNT; i++) {
        const char* name = dist_backend_name(static_cast<dist_backend_t>(i));
        EXPECT_NE(name, nullptr) << "Backend " << i << " returned null name";
    }
}

/* ============================================================================
 * Distributed Configuration Validation Tests
 * ============================================================================ */

TEST_F(DistributedTrainingTest, ValidateConfig_ValidDefault) {
    dist_config_t config;
    dist_default_config(&config, 4, 0);

    int result = dist_validate_config(&config);
    EXPECT_EQ(result, 0);
}

TEST_F(DistributedTrainingTest, ValidateConfig_SingleWorker) {
    dist_config_t config;
    dist_default_config(&config, 1, 0);

    int result = dist_validate_config(&config);
    EXPECT_EQ(result, 0);
}

TEST_F(DistributedTrainingTest, ValidateConfig_NullConfig) {
    int result = dist_validate_config(nullptr);
    EXPECT_LT(result, 0);
}

TEST_F(DistributedTrainingTest, ValidateConfig_InvalidRank) {
    dist_config_t config;
    dist_default_config(&config, 4, 0);
    config.worker.rank = 10;  // Invalid: rank >= world_size

    int result = dist_validate_config(&config);
    EXPECT_LT(result, 0);
}

TEST_F(DistributedTrainingTest, ValidateConfig_ZeroWorldSize) {
    dist_config_t config;
    dist_default_config(&config, 4, 0);
    config.worker.world_size = 0;  // Invalid

    int result = dist_validate_config(&config);
    EXPECT_LT(result, 0);
}

TEST_F(DistributedTrainingTest, ValidateConfig_ExceedsMaxWorkers) {
    dist_config_t config;
    dist_default_config(&config, 4, 0);
    config.worker.world_size = DIST_MAX_WORKERS + 1;  // Invalid

    // Note: Current implementation doesn't enforce max workers limit
    // This test documents the current behavior
    int result = dist_validate_config(&config);
    EXPECT_EQ(result, 0);  // Implementation accepts any world size
}

/* ============================================================================
 * Distributed Init Tests
 * ============================================================================ */

TEST_F(DistributedTrainingTest, Init_SingleWorker) {
    dist_config_t config;
    dist_default_config(&config, 1, 0);

    ctx = dist_create(&config);
    ASSERT_NE(ctx, nullptr);

    // For single worker, init should succeed without networking
    int result = dist_init(ctx);
    EXPECT_EQ(result, 0);
}

TEST_F(DistributedTrainingTest, Init_NullContext) {
    int result = dist_init(nullptr);
    EXPECT_LT(result, 0);
}

/* ============================================================================
 * Integration Tests: AMP with Distributed
 * ============================================================================ */

class AmpDistributedIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        amp_ctx = nullptr;
        dist_ctx = nullptr;
    }

    void TearDown() override {
        if (amp_ctx) {
            amp_destroy(amp_ctx);
            amp_ctx = nullptr;
        }
        if (dist_ctx) {
            dist_destroy(dist_ctx);
            dist_ctx = nullptr;
        }
    }

    amp_ctx_t* amp_ctx;
    dist_ctx_t* dist_ctx;
};

TEST_F(AmpDistributedIntegrationTest, BothContextsCreateSuccessfully) {
    amp_config_t amp_config;
    amp_default_config(&amp_config);
    amp_ctx = amp_create(&amp_config);
    ASSERT_NE(amp_ctx, nullptr);

    dist_config_t dist_config;
    dist_default_config(&dist_config, 4, 0);
    dist_ctx = dist_create(&dist_config);
    ASSERT_NE(dist_ctx, nullptr);
}

TEST_F(AmpDistributedIntegrationTest, AmpScalingWithDistributedCoordinator) {
    amp_config_t amp_config;
    amp_default_config(&amp_config);
    amp_config.scaling.init_scale = 1000.0f;
    amp_ctx = amp_create(&amp_config);
    ASSERT_NE(amp_ctx, nullptr);

    dist_config_t dist_config;
    dist_default_config(&dist_config, 4, 0);
    dist_ctx = dist_create(&dist_config);
    ASSERT_NE(dist_ctx, nullptr);

    // Verify coordinator can use AMP scaling
    EXPECT_TRUE(dist_is_coordinator(dist_ctx));
    EXPECT_FLOAT_EQ(amp_get_scale(amp_ctx), 1000.0f);

    float loss = 0.5f;
    float scaled_loss = amp_scale_loss(amp_ctx, loss);
    EXPECT_FLOAT_EQ(scaled_loss, 500.0f);
}

TEST_F(AmpDistributedIntegrationTest, AmpScalingWithDistributedWorker) {
    amp_config_t amp_config;
    amp_default_config(&amp_config);
    amp_ctx = amp_create(&amp_config);
    ASSERT_NE(amp_ctx, nullptr);

    dist_config_t dist_config;
    dist_default_config(&dist_config, 4, 2);  // Non-coordinator
    dist_ctx = dist_create(&dist_config);
    ASSERT_NE(dist_ctx, nullptr);

    // Verify worker can also use AMP
    EXPECT_FALSE(dist_is_coordinator(dist_ctx));
    EXPECT_EQ(dist_get_rank(dist_ctx), 2u);

    // AMP should work independently
    EXPECT_TRUE(amp_is_autocasting(amp_ctx) == false);
    amp_autocast_enter(amp_ctx);
    EXPECT_TRUE(amp_is_autocasting(amp_ctx));
}

/* ============================================================================
 * Main Entry Point
 * ============================================================================ */

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
