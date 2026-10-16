/**
 * @file test_gradient_manager.cpp
 * @brief Unit tests for Gradient Management module
 *
 * Tests:
 * - Gradient accumulation
 * - Gradient scaling (fixed and dynamic)
 * - Health checking (NaN/Inf detection)
 * - Gradient statistics
 * - Distributed training support
 *
 * @note Part of Phase TM-6: Gradient Management
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <limits>

// Headers have their own extern "C" guards
#include "middleware/training/nimcp_gradient_manager.h"

/* ============================================================================
 * Test Fixtures
 * ============================================================================ */

class GradientManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        ctx = nullptr;
    }

    void TearDown() override {
        if (ctx) {
            nimcp_gradient_manager_destroy(ctx);
            ctx = nullptr;
        }
    }

    nimcp_gradient_manager_ctx_t* ctx;
};

/* ============================================================================
 * Default Configuration Tests
 * ============================================================================ */

TEST_F(GradientManagerTest, DefaultConfig_ReasonableValues) {
    nimcp_gradient_manager_config_t config = nimcp_gradient_manager_default_config();

    EXPECT_FALSE(config.use_accumulation);
    EXPECT_FALSE(config.use_scaling);
    EXPECT_TRUE(config.check_nan_inf);
    EXPECT_TRUE(config.skip_nan_gradients);
    EXPECT_TRUE(config.track_statistics);
}

TEST_F(GradientManagerTest, AccumConfig_DefaultValues) {
    nimcp_grad_accum_config_t config = nimcp_grad_accum_default_config(4);

    EXPECT_EQ(config.accumulation_steps, 4u);
    EXPECT_EQ(config.mode, NIMCP_GRAD_ACCUM_SUM);
}

TEST_F(GradientManagerTest, ScaleConfig_DefaultValues) {
    nimcp_grad_scale_config_t config = nimcp_grad_scale_default_config(65536.0f);

    EXPECT_EQ(config.strategy, NIMCP_GRAD_SCALE_DYNAMIC);
    EXPECT_FLOAT_EQ(config.initial_scale, 65536.0f);
    EXPECT_FLOAT_EQ(config.backoff_factor, NIMCP_GRAD_BACKOFF_FACTOR);
    EXPECT_FLOAT_EQ(config.growth_factor, NIMCP_GRAD_GROWTH_FACTOR);
}

/* ============================================================================
 * Creation and Destruction Tests
 * ============================================================================ */

TEST_F(GradientManagerTest, Create_DefaultConfig) {
    nimcp_gradient_manager_config_t config = nimcp_gradient_manager_default_config();
    ctx = nimcp_gradient_manager_create(&config);
    ASSERT_NE(ctx, nullptr);
}

TEST_F(GradientManagerTest, Create_WithAccumulation) {
    nimcp_gradient_manager_config_t config = nimcp_gradient_manager_default_config();
    config.use_accumulation = true;
    config.accumulation = nimcp_grad_accum_default_config(4);

    ctx = nimcp_gradient_manager_create(&config);
    ASSERT_NE(ctx, nullptr);
}

TEST_F(GradientManagerTest, Create_WithScaling) {
    nimcp_gradient_manager_config_t config = nimcp_gradient_manager_default_config();
    config.use_scaling = true;
    config.scaling = nimcp_grad_scale_default_config(1024.0f);

    ctx = nimcp_gradient_manager_create(&config);
    ASSERT_NE(ctx, nullptr);

    EXPECT_FLOAT_EQ(nimcp_gradient_get_scale(ctx), 1024.0f);
}

TEST_F(GradientManagerTest, Create_RejectsInvalidConfig) {
    nimcp_gradient_manager_config_t config = nimcp_gradient_manager_default_config();
    config.use_accumulation = true;
    config.accumulation.accumulation_steps = 0;  // Invalid

    ctx = nimcp_gradient_manager_create(&config);
    EXPECT_EQ(ctx, nullptr);
}

/* ============================================================================
 * Gradient Accumulation Tests
 * ============================================================================ */

TEST_F(GradientManagerTest, Accumulation_SumMode) {
    nimcp_gradient_manager_config_t config = nimcp_gradient_manager_default_config();
    config.use_accumulation = true;
    config.accumulation.accumulation_steps = 4;
    config.accumulation.mode = NIMCP_GRAD_ACCUM_SUM;

    ctx = nimcp_gradient_manager_create(&config);
    ASSERT_NE(ctx, nullptr);

    std::vector<float> grads1 = {1.0f, 2.0f, 3.0f};
    std::vector<float> grads2 = {0.5f, 1.0f, 1.5f};
    std::vector<float> grads3 = {0.25f, 0.5f, 0.75f};
    std::vector<float> grads4 = {0.25f, 0.5f, 0.75f};

    EXPECT_FALSE(nimcp_gradient_accum_ready(ctx));

    nimcp_gradient_accumulate(ctx, grads1.data(), grads1.size());
    EXPECT_FALSE(nimcp_gradient_accum_ready(ctx));

    nimcp_gradient_accumulate(ctx, grads2.data(), grads2.size());
    nimcp_gradient_accumulate(ctx, grads3.data(), grads3.size());
    nimcp_gradient_accumulate(ctx, grads4.data(), grads4.size());

    EXPECT_TRUE(nimcp_gradient_accum_ready(ctx));

    std::vector<float> output(3);
    nimcp_gradient_get_accumulated(ctx, output.data(), output.size());

    // Sum: 1+0.5+0.25+0.25 = 2, 2+1+0.5+0.5 = 4, 3+1.5+0.75+0.75 = 6
    EXPECT_FLOAT_EQ(output[0], 2.0f);
    EXPECT_FLOAT_EQ(output[1], 4.0f);
    EXPECT_FLOAT_EQ(output[2], 6.0f);
}

TEST_F(GradientManagerTest, Accumulation_MeanMode) {
    nimcp_gradient_manager_config_t config = nimcp_gradient_manager_default_config();
    config.use_accumulation = true;
    config.accumulation.accumulation_steps = 4;
    config.accumulation.mode = NIMCP_GRAD_ACCUM_MEAN;

    ctx = nimcp_gradient_manager_create(&config);
    ASSERT_NE(ctx, nullptr);

    std::vector<float> grads = {4.0f, 8.0f, 12.0f};

    for (int i = 0; i < 4; i++) {
        nimcp_gradient_accumulate(ctx, grads.data(), grads.size());
    }

    std::vector<float> output(3);
    nimcp_gradient_get_accumulated(ctx, output.data(), output.size());

    // Mean: sum / 4 = (4*4)/4 = 4, (8*4)/4 = 8, (12*4)/4 = 12
    EXPECT_FLOAT_EQ(output[0], 4.0f);
    EXPECT_FLOAT_EQ(output[1], 8.0f);
    EXPECT_FLOAT_EQ(output[2], 12.0f);
}

TEST_F(GradientManagerTest, Accumulation_Reset) {
    nimcp_gradient_manager_config_t config = nimcp_gradient_manager_default_config();
    config.use_accumulation = true;
    config.accumulation.accumulation_steps = 2;

    ctx = nimcp_gradient_manager_create(&config);
    ASSERT_NE(ctx, nullptr);

    std::vector<float> grads = {1.0f, 2.0f, 3.0f};
    nimcp_gradient_accumulate(ctx, grads.data(), grads.size());

    EXPECT_EQ(nimcp_gradient_get_accum_step(ctx), 1u);

    nimcp_gradient_reset_accum(ctx);

    EXPECT_EQ(nimcp_gradient_get_accum_step(ctx), 0u);
}

TEST_F(GradientManagerTest, Accumulation_SkipsNaN) {
    nimcp_gradient_manager_config_t config = nimcp_gradient_manager_default_config();
    config.use_accumulation = true;
    config.accumulation.accumulation_steps = 3;
    config.check_nan_inf = true;
    config.skip_nan_gradients = true;

    ctx = nimcp_gradient_manager_create(&config);
    ASSERT_NE(ctx, nullptr);

    std::vector<float> good_grads = {1.0f, 2.0f, 3.0f};
    std::vector<float> nan_grads = {1.0f, std::numeric_limits<float>::quiet_NaN(), 3.0f};

    nimcp_gradient_accumulate(ctx, good_grads.data(), good_grads.size());
    nimcp_gradient_accumulate(ctx, nan_grads.data(), nan_grads.size());  // Skipped
    nimcp_gradient_accumulate(ctx, good_grads.data(), good_grads.size());

    // Only 2 valid accumulations
    EXPECT_EQ(nimcp_gradient_get_accum_step(ctx), 2u);
}

/* ============================================================================
 * Gradient Scaling Tests
 * ============================================================================ */

TEST_F(GradientManagerTest, Scale_FixedScaling) {
    nimcp_gradient_manager_config_t config = nimcp_gradient_manager_default_config();
    config.use_scaling = true;
    config.scaling.strategy = NIMCP_GRAD_SCALE_FIXED;
    config.scaling.initial_scale = 100.0f;

    ctx = nimcp_gradient_manager_create(&config);
    ASSERT_NE(ctx, nullptr);

    std::vector<float> grads = {1.0f, 2.0f, 3.0f};
    float scale = nimcp_gradient_scale(ctx, grads.data(), grads.size());

    EXPECT_FLOAT_EQ(scale, 100.0f);
    EXPECT_FLOAT_EQ(grads[0], 100.0f);
    EXPECT_FLOAT_EQ(grads[1], 200.0f);
    EXPECT_FLOAT_EQ(grads[2], 300.0f);
}

TEST_F(GradientManagerTest, Scale_Unscale) {
    nimcp_gradient_manager_config_t config = nimcp_gradient_manager_default_config();
    config.use_scaling = true;
    config.scaling.strategy = NIMCP_GRAD_SCALE_FIXED;
    config.scaling.initial_scale = 100.0f;

    ctx = nimcp_gradient_manager_create(&config);
    ASSERT_NE(ctx, nullptr);

    std::vector<float> original = {1.0f, 2.0f, 3.0f};
    std::vector<float> grads = original;

    nimcp_gradient_scale(ctx, grads.data(), grads.size());
    float inv_scale = nimcp_gradient_unscale(ctx, grads.data(), grads.size());

    EXPECT_FLOAT_EQ(inv_scale, 0.01f);
    EXPECT_FLOAT_EQ(grads[0], original[0]);
    EXPECT_FLOAT_EQ(grads[1], original[1]);
    EXPECT_FLOAT_EQ(grads[2], original[2]);
}

TEST_F(GradientManagerTest, Scale_SetManually) {
    nimcp_gradient_manager_config_t config = nimcp_gradient_manager_default_config();
    config.use_scaling = true;
    config.scaling = nimcp_grad_scale_default_config(1000.0f);

    ctx = nimcp_gradient_manager_create(&config);
    ASSERT_NE(ctx, nullptr);

    EXPECT_FLOAT_EQ(nimcp_gradient_get_scale(ctx), 1000.0f);

    nimcp_result_t result = nimcp_gradient_set_scale(ctx, 500.0f);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_FLOAT_EQ(nimcp_gradient_get_scale(ctx), 500.0f);
}

TEST_F(GradientManagerTest, Scale_DynamicBackoff) {
    nimcp_gradient_manager_config_t config = nimcp_gradient_manager_default_config();
    config.use_scaling = true;
    config.scaling.strategy = NIMCP_GRAD_SCALE_DYNAMIC;
    config.scaling.initial_scale = 1000.0f;
    config.scaling.backoff_factor = 0.5f;

    ctx = nimcp_gradient_manager_create(&config);
    ASSERT_NE(ctx, nullptr);

    EXPECT_FLOAT_EQ(nimcp_gradient_get_scale(ctx), 1000.0f);

    // Simulate overflow
    nimcp_gradient_update_scale(ctx, NIMCP_GRAD_HAS_INF);

    EXPECT_FLOAT_EQ(nimcp_gradient_get_scale(ctx), 500.0f);

    // Another overflow
    nimcp_gradient_update_scale(ctx, NIMCP_GRAD_HAS_INF);
    EXPECT_FLOAT_EQ(nimcp_gradient_get_scale(ctx), 250.0f);
}

TEST_F(GradientManagerTest, Scale_DynamicGrowth) {
    nimcp_gradient_manager_config_t config = nimcp_gradient_manager_default_config();
    config.use_scaling = true;
    config.scaling.strategy = NIMCP_GRAD_SCALE_DYNAMIC;
    config.scaling.initial_scale = 100.0f;
    config.scaling.growth_factor = 2.0f;
    config.scaling.growth_interval = 3;

    ctx = nimcp_gradient_manager_create(&config);
    ASSERT_NE(ctx, nullptr);

    // Simulate healthy gradients for growth_interval steps
    for (int i = 0; i < 3; i++) {
        nimcp_gradient_update_scale(ctx, NIMCP_GRAD_HEALTHY);
    }

    EXPECT_FLOAT_EQ(nimcp_gradient_get_scale(ctx), 200.0f);
}

/* ============================================================================
 * Health Checking Tests
 * ============================================================================ */

TEST_F(GradientManagerTest, Health_DetectsNaN) {
    std::vector<float> grads = {1.0f, std::numeric_limits<float>::quiet_NaN(), 3.0f};

    nimcp_grad_health_t health = nimcp_gradient_check_health(grads.data(), grads.size());
    EXPECT_EQ(health, NIMCP_GRAD_HAS_NAN);
}

TEST_F(GradientManagerTest, Health_DetectsInf) {
    std::vector<float> grads = {1.0f, std::numeric_limits<float>::infinity(), 3.0f};

    nimcp_grad_health_t health = nimcp_gradient_check_health(grads.data(), grads.size());
    EXPECT_EQ(health, NIMCP_GRAD_HAS_INF);
}

TEST_F(GradientManagerTest, Health_DetectsNegInf) {
    std::vector<float> grads = {1.0f, -std::numeric_limits<float>::infinity(), 3.0f};

    nimcp_grad_health_t health = nimcp_gradient_check_health(grads.data(), grads.size());
    EXPECT_EQ(health, NIMCP_GRAD_HAS_INF);
}

TEST_F(GradientManagerTest, Health_DetectsZero) {
    std::vector<float> grads = {0.0f, 0.0f, 0.0f};

    nimcp_grad_health_t health = nimcp_gradient_check_health(grads.data(), grads.size());
    EXPECT_EQ(health, NIMCP_GRAD_HAS_ZERO);
}

TEST_F(GradientManagerTest, Health_Healthy) {
    std::vector<float> grads = {1.0f, -2.0f, 0.5f};

    nimcp_grad_health_t health = nimcp_gradient_check_health(grads.data(), grads.size());
    EXPECT_EQ(health, NIMCP_GRAD_HEALTHY);
}

TEST_F(GradientManagerTest, Sanitize_ReplacesNaN) {
    std::vector<float> grads = {1.0f, std::numeric_limits<float>::quiet_NaN(), 3.0f};

    uint64_t replaced = nimcp_gradient_sanitize(grads.data(), grads.size(), 0.0f);

    EXPECT_EQ(replaced, 1u);
    EXPECT_FLOAT_EQ(grads[0], 1.0f);
    EXPECT_FLOAT_EQ(grads[1], 0.0f);
    EXPECT_FLOAT_EQ(grads[2], 3.0f);
}

TEST_F(GradientManagerTest, Sanitize_ReplacesInf) {
    std::vector<float> grads = {
        std::numeric_limits<float>::infinity(),
        1.0f,
        -std::numeric_limits<float>::infinity()
    };

    uint64_t replaced = nimcp_gradient_sanitize(grads.data(), grads.size(), 0.0f);

    EXPECT_EQ(replaced, 2u);
    EXPECT_FLOAT_EQ(grads[0], 0.0f);
    EXPECT_FLOAT_EQ(grads[1], 1.0f);
    EXPECT_FLOAT_EQ(grads[2], 0.0f);
}

/* ============================================================================
 * Gradient Norm Tests
 * ============================================================================ */

TEST_F(GradientManagerTest, L2Norm_Computes) {
    std::vector<float> grads = {3.0f, 4.0f};

    float norm = nimcp_gradient_l2_norm(grads.data(), grads.size());
    EXPECT_FLOAT_EQ(norm, 5.0f);
}

TEST_F(GradientManagerTest, L1Norm_Computes) {
    std::vector<float> grads = {1.0f, -2.0f, 3.0f, -4.0f};

    float norm = nimcp_gradient_l1_norm(grads.data(), grads.size());
    EXPECT_FLOAT_EQ(norm, 10.0f);
}

TEST_F(GradientManagerTest, MaxNorm_Computes) {
    std::vector<float> grads = {1.0f, -5.0f, 3.0f, -2.0f};

    float norm = nimcp_gradient_max_norm(grads.data(), grads.size());
    EXPECT_FLOAT_EQ(norm, 5.0f);
}

/* ============================================================================
 * Statistics Tests
 * ============================================================================ */

TEST_F(GradientManagerTest, Stats_TracksSteps) {
    nimcp_gradient_manager_config_t config = nimcp_gradient_manager_default_config();
    config.use_accumulation = true;
    config.accumulation.accumulation_steps = 2;
    config.track_statistics = true;

    ctx = nimcp_gradient_manager_create(&config);
    ASSERT_NE(ctx, nullptr);

    std::vector<float> grads = {1.0f, 2.0f, 3.0f};
    std::vector<float> output(3);

    nimcp_gradient_accumulate(ctx, grads.data(), grads.size());
    nimcp_gradient_accumulate(ctx, grads.data(), grads.size());
    nimcp_gradient_get_accumulated(ctx, output.data(), output.size());

    nimcp_grad_stats_t stats;
    nimcp_gradient_manager_get_stats(ctx, &stats);

    EXPECT_EQ(stats.total_steps, 1u);
    EXPECT_EQ(stats.total_accum_steps, 2u);
}

TEST_F(GradientManagerTest, Stats_TracksScaling) {
    nimcp_gradient_manager_config_t config = nimcp_gradient_manager_default_config();
    config.use_scaling = true;
    config.scaling.strategy = NIMCP_GRAD_SCALE_DYNAMIC;
    config.scaling.initial_scale = 1000.0f;
    config.scaling.growth_interval = 1;

    ctx = nimcp_gradient_manager_create(&config);
    ASSERT_NE(ctx, nullptr);

    // Trigger overflow
    nimcp_gradient_update_scale(ctx, NIMCP_GRAD_HAS_INF);

    // Healthy growth
    nimcp_gradient_update_scale(ctx, NIMCP_GRAD_HEALTHY);

    nimcp_grad_stats_t stats;
    nimcp_gradient_manager_get_stats(ctx, &stats);

    EXPECT_EQ(stats.scale_decreases, 1u);
    EXPECT_EQ(stats.scale_increases, 1u);
    EXPECT_EQ(stats.overflow_count, 1u);
}

TEST_F(GradientManagerTest, Stats_Reset) {
    nimcp_gradient_manager_config_t config = nimcp_gradient_manager_default_config();
    config.use_scaling = true;
    config.scaling.strategy = NIMCP_GRAD_SCALE_DYNAMIC;
    config.scaling.initial_scale = 1000.0f;

    ctx = nimcp_gradient_manager_create(&config);
    ASSERT_NE(ctx, nullptr);

    nimcp_gradient_update_scale(ctx, NIMCP_GRAD_HAS_INF);

    nimcp_gradient_manager_reset_stats(ctx);

    nimcp_grad_stats_t stats;
    nimcp_gradient_manager_get_stats(ctx, &stats);

    EXPECT_EQ(stats.scale_decreases, 0u);
    EXPECT_EQ(stats.overflow_count, 0u);
}

/* ============================================================================
 * Distributed Training Tests
 * ============================================================================ */

TEST_F(GradientManagerTest, AllReduce_Finalize) {
    std::vector<float> grads = {10.0f, 20.0f, 30.0f};
    uint32_t num_workers = 4;

    // Simulate after all-reduce sum
    nimcp_gradient_finalize_allreduce(grads.data(), grads.size(), num_workers);

    // Should be averaged
    EXPECT_FLOAT_EQ(grads[0], 2.5f);
    EXPECT_FLOAT_EQ(grads[1], 5.0f);
    EXPECT_FLOAT_EQ(grads[2], 7.5f);
}

/* ============================================================================
 * Utility Function Tests
 * ============================================================================ */

TEST_F(GradientManagerTest, AccumModeNames) {
    EXPECT_STREQ(nimcp_grad_accum_mode_name(NIMCP_GRAD_ACCUM_SUM), "Sum");
    EXPECT_STREQ(nimcp_grad_accum_mode_name(NIMCP_GRAD_ACCUM_MEAN), "Mean");
}

TEST_F(GradientManagerTest, ScaleStrategyNames) {
    EXPECT_STREQ(nimcp_grad_scale_strategy_name(NIMCP_GRAD_SCALE_NONE), "None");
    EXPECT_STREQ(nimcp_grad_scale_strategy_name(NIMCP_GRAD_SCALE_FIXED), "Fixed");
    EXPECT_STREQ(nimcp_grad_scale_strategy_name(NIMCP_GRAD_SCALE_DYNAMIC), "Dynamic");
}

TEST_F(GradientManagerTest, HealthNames) {
    EXPECT_STREQ(nimcp_grad_health_name(NIMCP_GRAD_HEALTHY), "Healthy");
    EXPECT_STREQ(nimcp_grad_health_name(NIMCP_GRAD_HAS_NAN), "Has NaN");
    EXPECT_STREQ(nimcp_grad_health_name(NIMCP_GRAD_HAS_INF), "Has Inf");
    EXPECT_STREQ(nimcp_grad_health_name(NIMCP_GRAD_HAS_ZERO), "All Zero");
}

/* ============================================================================
 * Validation Tests
 * ============================================================================ */

TEST_F(GradientManagerTest, Validation_ValidConfig) {
    nimcp_gradient_manager_config_t config = nimcp_gradient_manager_default_config();

    nimcp_result_t result = nimcp_gradient_manager_validate_config(&config);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(GradientManagerTest, Validation_InvalidAccumSteps) {
    nimcp_gradient_manager_config_t config = nimcp_gradient_manager_default_config();
    config.use_accumulation = true;
    config.accumulation.accumulation_steps = NIMCP_GRAD_MAX_ACCUM_STEPS + 1;

    nimcp_result_t result = nimcp_gradient_manager_validate_config(&config);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(GradientManagerTest, Validation_InvalidScaleConfig) {
    nimcp_gradient_manager_config_t config = nimcp_gradient_manager_default_config();
    config.use_scaling = true;
    config.scaling.strategy = NIMCP_GRAD_SCALE_DYNAMIC;
    config.scaling.initial_scale = -1.0f;  // Invalid

    nimcp_result_t result = nimcp_gradient_manager_validate_config(&config);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(GradientManagerTest, Validation_InvalidBackoffFactor) {
    nimcp_gradient_manager_config_t config = nimcp_gradient_manager_default_config();
    config.use_scaling = true;
    config.scaling.strategy = NIMCP_GRAD_SCALE_DYNAMIC;
    config.scaling.initial_scale = 1000.0f;
    config.scaling.min_scale = 1.0f;
    config.scaling.max_scale = 10000.0f;
    config.scaling.backoff_factor = 1.5f;  // Invalid: must be < 1

    nimcp_result_t result = nimcp_gradient_manager_validate_config(&config);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

/* ============================================================================
 * Edge Case Tests
 * ============================================================================ */

TEST_F(GradientManagerTest, NullContext_HandledGracefully) {
    EXPECT_FLOAT_EQ(nimcp_gradient_get_scale(nullptr), 1.0f);
    EXPECT_EQ(nimcp_gradient_get_accum_step(nullptr), 0u);
    EXPECT_FALSE(nimcp_gradient_accum_ready(nullptr));

    nimcp_grad_stats_t stats;
    EXPECT_EQ(nimcp_gradient_manager_get_stats(nullptr, &stats), NIMCP_ERROR_INVALID_PARAM);

    // Should not crash
    nimcp_gradient_manager_destroy(nullptr);
    nimcp_gradient_manager_reset_stats(nullptr);
    nimcp_gradient_reset_accum(nullptr);
}

TEST_F(GradientManagerTest, EmptyArrays_HandledGracefully) {
    std::vector<float> empty;

    EXPECT_FLOAT_EQ(nimcp_gradient_l2_norm(empty.data(), 0), 0.0f);
    EXPECT_FLOAT_EQ(nimcp_gradient_l1_norm(empty.data(), 0), 0.0f);
    EXPECT_FLOAT_EQ(nimcp_gradient_max_norm(empty.data(), 0), 0.0f);
    EXPECT_EQ(nimcp_gradient_check_health(empty.data(), 0), NIMCP_GRAD_HEALTHY);
    EXPECT_EQ(nimcp_gradient_sanitize(empty.data(), 0, 0.0f), 0u);
}

TEST_F(GradientManagerTest, NoScaling_ReturnsOne) {
    nimcp_gradient_manager_config_t config = nimcp_gradient_manager_default_config();
    config.use_scaling = false;

    ctx = nimcp_gradient_manager_create(&config);
    ASSERT_NE(ctx, nullptr);

    std::vector<float> grads = {1.0f, 2.0f, 3.0f};
    std::vector<float> original = grads;

    float scale = nimcp_gradient_scale(ctx, grads.data(), grads.size());

    EXPECT_FLOAT_EQ(scale, 1.0f);
    EXPECT_FLOAT_EQ(grads[0], original[0]);
    EXPECT_FLOAT_EQ(grads[1], original[1]);
    EXPECT_FLOAT_EQ(grads[2], original[2]);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
