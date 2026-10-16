/**
 * @file test_regularization.cpp
 * @brief Unit tests for Regularization module
 *
 * Tests:
 * - L1/L2/Elastic Net weight regularization
 * - Gradient clipping (by value, by norm)
 * - Dropout (standard)
 * - Label smoothing
 * - Early stopping
 *
 * @note Part of Phase TM-5: Regularization
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <numeric>

// Headers have their own extern "C" guards
#include "middleware/training/nimcp_regularization.h"

/* ============================================================================
 * Test Fixtures
 * ============================================================================ */

class RegularizationTest : public ::testing::Test {
protected:
    void SetUp() override {
        ctx = nullptr;
        dropout_ctx = nullptr;
        early_stop_ctx = nullptr;
    }

    void TearDown() override {
        if (ctx) {
            nimcp_regularization_destroy(ctx);
            ctx = nullptr;
        }
        if (dropout_ctx) {
            nimcp_dropout_destroy(dropout_ctx);
            dropout_ctx = nullptr;
        }
        if (early_stop_ctx) {
            nimcp_early_stop_destroy(early_stop_ctx);
            early_stop_ctx = nullptr;
        }
    }

    nimcp_regularization_ctx_t* ctx;
    nimcp_dropout_ctx_t* dropout_ctx;
    nimcp_early_stop_ctx_t* early_stop_ctx;
};

/* ============================================================================
 * L1 Regularization Tests
 * ============================================================================ */

TEST_F(RegularizationTest, L1Loss_ComputesCorrectly) {
    std::vector<float> weights = {1.0f, -2.0f, 3.0f, -4.0f, 5.0f};
    float lambda = 0.1f;

    // L1 loss = lambda * sum(|w|) = 0.1 * (1 + 2 + 3 + 4 + 5) = 1.5
    float loss = nimcp_l1_loss(weights.data(), weights.size(), lambda);
    EXPECT_FLOAT_EQ(loss, 1.5f);
}

TEST_F(RegularizationTest, L1Loss_ZeroWeights) {
    std::vector<float> weights = {0.0f, 0.0f, 0.0f};
    float loss = nimcp_l1_loss(weights.data(), weights.size(), 0.1f);
    EXPECT_FLOAT_EQ(loss, 0.0f);
}

TEST_F(RegularizationTest, L1Gradient_AddsSign) {
    std::vector<float> weights = {1.0f, -2.0f, 0.0f, 3.0f};
    std::vector<float> gradients = {0.0f, 0.0f, 0.0f, 0.0f};
    float lambda = 0.1f;

    nimcp_l1_gradient(weights.data(), gradients.data(), weights.size(), lambda);

    // Gradient should be lambda * sign(w)
    EXPECT_FLOAT_EQ(gradients[0], 0.1f);   // sign(1) = 1
    EXPECT_FLOAT_EQ(gradients[1], -0.1f);  // sign(-2) = -1
    EXPECT_FLOAT_EQ(gradients[2], 0.0f);   // sign(0) = 0
    EXPECT_FLOAT_EQ(gradients[3], 0.1f);   // sign(3) = 1
}

/* ============================================================================
 * L2 Regularization Tests
 * ============================================================================ */

TEST_F(RegularizationTest, L2Loss_ComputesCorrectly) {
    std::vector<float> weights = {1.0f, 2.0f, 3.0f};
    float lambda = 0.1f;

    // L2 loss = 0.5 * lambda * sum(w^2) = 0.5 * 0.1 * (1 + 4 + 9) = 0.7
    float loss = nimcp_l2_loss(weights.data(), weights.size(), lambda);
    EXPECT_FLOAT_EQ(loss, 0.7f);
}

TEST_F(RegularizationTest, L2Gradient_AddsWeight) {
    std::vector<float> weights = {1.0f, -2.0f, 3.0f};
    std::vector<float> gradients = {0.0f, 0.0f, 0.0f};
    float lambda = 0.1f;

    nimcp_l2_gradient(weights.data(), gradients.data(), weights.size(), lambda);

    // Gradient should be lambda * w
    EXPECT_FLOAT_EQ(gradients[0], 0.1f);   // 0.1 * 1
    EXPECT_FLOAT_EQ(gradients[1], -0.2f);  // 0.1 * -2
    EXPECT_FLOAT_EQ(gradients[2], 0.3f);   // 0.1 * 3
}

/* ============================================================================
 * Elastic Net Tests
 * ============================================================================ */

TEST_F(RegularizationTest, ElasticNet_CombinesL1L2) {
    std::vector<float> weights = {1.0f, 2.0f, 3.0f};
    float lambda = 0.1f;
    float alpha = 0.5f;  // 50% L1, 50% L2

    float l1_loss = nimcp_l1_loss(weights.data(), weights.size(), 1.0f);  // 6
    float l2_loss = nimcp_l2_loss(weights.data(), weights.size(), 1.0f);  // 7

    // Elastic = lambda * (alpha * L1 + (1-alpha) * L2)
    float expected = 0.1f * (0.5f * 6.0f + 0.5f * 7.0f);  // 0.65
    float actual = nimcp_elastic_net_loss(weights.data(), weights.size(), lambda, alpha);

    EXPECT_NEAR(actual, expected, 1e-6f);
}

TEST_F(RegularizationTest, ElasticNet_PureL1) {
    std::vector<float> weights = {1.0f, 2.0f, 3.0f};
    float lambda = 0.1f;

    float l1_only = nimcp_elastic_net_loss(weights.data(), weights.size(), lambda, 1.0f);
    float l1_direct = nimcp_l1_loss(weights.data(), weights.size(), lambda);

    EXPECT_NEAR(l1_only, l1_direct, 1e-6f);
}

TEST_F(RegularizationTest, ElasticNet_PureL2) {
    std::vector<float> weights = {1.0f, 2.0f, 3.0f};
    float lambda = 0.1f;

    float l2_only = nimcp_elastic_net_loss(weights.data(), weights.size(), lambda, 0.0f);
    float l2_direct = nimcp_l2_loss(weights.data(), weights.size(), lambda);

    EXPECT_NEAR(l2_only, l2_direct, 1e-6f);
}

/* ============================================================================
 * Regularization Context Tests
 * ============================================================================ */

TEST_F(RegularizationTest, Context_CreationL2) {
    nimcp_regularization_config_t config = nimcp_regularization_default_config();
    config.weight_reg_type = NIMCP_REG_L2;
    config.weight_reg.l2.lambda = 0.01f;

    ctx = nimcp_regularization_create(&config);
    ASSERT_NE(ctx, nullptr);

    std::vector<float> weights = {1.0f, 2.0f, 3.0f};
    float loss = nimcp_regularization_loss(ctx, weights.data(), weights.size());

    float expected = nimcp_l2_loss(weights.data(), weights.size(), 0.01f);
    EXPECT_FLOAT_EQ(loss, expected);
}

TEST_F(RegularizationTest, Context_ApplyGradient) {
    nimcp_regularization_config_t config = nimcp_regularization_default_config();
    config.weight_reg_type = NIMCP_REG_L2;
    config.weight_reg.l2.lambda = 0.1f;

    ctx = nimcp_regularization_create(&config);
    ASSERT_NE(ctx, nullptr);

    std::vector<float> weights = {1.0f, 2.0f, 3.0f};
    std::vector<float> gradients = {0.5f, 0.5f, 0.5f};

    nimcp_result_t result = nimcp_regularization_apply_gradient(
        ctx, weights.data(), gradients.data(), weights.size()
    );
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Gradient should be base + lambda * w
    EXPECT_FLOAT_EQ(gradients[0], 0.5f + 0.1f * 1.0f);  // 0.6
    EXPECT_FLOAT_EQ(gradients[1], 0.5f + 0.1f * 2.0f);  // 0.7
    EXPECT_FLOAT_EQ(gradients[2], 0.5f + 0.1f * 3.0f);  // 0.8
}

/* ============================================================================
 * Gradient Clipping Tests
 * ============================================================================ */

TEST_F(RegularizationTest, ClipByValue_ClipsCorrectly) {
    std::vector<float> gradients = {0.5f, -2.0f, 1.5f, -0.3f, 3.0f};
    float threshold = 1.0f;

    uint64_t clipped = nimcp_gradient_clip_by_value(
        gradients.data(), gradients.size(), threshold
    );

    EXPECT_EQ(clipped, 3u);  // -2.0, 1.5, 3.0 were clipped
    EXPECT_FLOAT_EQ(gradients[0], 0.5f);   // No change
    EXPECT_FLOAT_EQ(gradients[1], -1.0f);  // Clipped from -2.0
    EXPECT_FLOAT_EQ(gradients[2], 1.0f);   // Clipped from 1.5
    EXPECT_FLOAT_EQ(gradients[3], -0.3f);  // No change
    EXPECT_FLOAT_EQ(gradients[4], 1.0f);   // Clipped from 3.0
}

TEST_F(RegularizationTest, ClipByNorm_ScalesWhenExceeded) {
    std::vector<float> gradients = {3.0f, 4.0f};  // Norm = 5
    float max_norm = 2.5f;

    float ratio = nimcp_gradient_clip_by_norm(
        gradients.data(), gradients.size(), max_norm
    );

    EXPECT_FLOAT_EQ(ratio, 0.5f);  // 2.5 / 5.0

    float new_norm = nimcp_gradient_norm(gradients.data(), gradients.size());
    EXPECT_NEAR(new_norm, 2.5f, 1e-5f);
}

TEST_F(RegularizationTest, ClipByNorm_NoChangeWhenUnderThreshold) {
    std::vector<float> gradients = {0.3f, 0.4f};  // Norm = 0.5

    float ratio = nimcp_gradient_clip_by_norm(
        gradients.data(), gradients.size(), 1.0f
    );

    EXPECT_FLOAT_EQ(ratio, 1.0f);
    EXPECT_FLOAT_EQ(gradients[0], 0.3f);
    EXPECT_FLOAT_EQ(gradients[1], 0.4f);
}

TEST_F(RegularizationTest, GradientNorm_ComputesL2) {
    std::vector<float> gradients = {3.0f, 4.0f};
    float norm = nimcp_gradient_norm(gradients.data(), gradients.size());
    EXPECT_FLOAT_EQ(norm, 5.0f);
}

TEST_F(RegularizationTest, Context_GradientClipByNorm) {
    nimcp_regularization_config_t config = nimcp_regularization_default_config();
    config.gradient_clip.mode = NIMCP_CLIP_BY_NORM;
    config.gradient_clip.threshold = 1.0f;

    ctx = nimcp_regularization_create(&config);
    ASSERT_NE(ctx, nullptr);

    std::vector<float> gradients = {3.0f, 4.0f};  // Norm = 5

    float ratio = nimcp_gradient_clip(ctx, gradients.data(), gradients.size());

    EXPECT_FLOAT_EQ(ratio, 0.2f);  // 1.0 / 5.0

    float new_norm = nimcp_gradient_norm(gradients.data(), gradients.size());
    EXPECT_NEAR(new_norm, 1.0f, 1e-5f);
}

/* ============================================================================
 * Dropout Tests
 * ============================================================================ */

TEST_F(RegularizationTest, Dropout_CreationAndDestruction) {
    nimcp_dropout_config_t config = nimcp_dropout_default_config(0.5f);
    config.seed = 12345;

    dropout_ctx = nimcp_dropout_create(&config);
    ASSERT_NE(dropout_ctx, nullptr);

    EXPECT_FLOAT_EQ(nimcp_dropout_get_rate(dropout_ctx), 0.5f);
}

TEST_F(RegularizationTest, Dropout_DropsSomeNeurons) {
    nimcp_dropout_config_t config = nimcp_dropout_default_config(0.5f);
    config.seed = 42;
    config.training = true;

    dropout_ctx = nimcp_dropout_create(&config);
    ASSERT_NE(dropout_ctx, nullptr);

    std::vector<float> activations(1000, 1.0f);
    std::vector<uint8_t> mask(1000);

    nimcp_result_t result = nimcp_dropout_forward(
        dropout_ctx, activations.data(), activations.size(), mask.data()
    );
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Count zeros
    int zeros = std::count_if(activations.begin(), activations.end(),
                              [](float v) { return v == 0.0f; });

    // With 50% dropout, expect ~500 zeros (allow 20% variance)
    EXPECT_GT(zeros, 300);
    EXPECT_LT(zeros, 700);

    // Non-zero values should be scaled by 1/(1-rate) = 2
    for (size_t i = 0; i < activations.size(); i++) {
        if (mask[i]) {
            EXPECT_NEAR(activations[i], 2.0f, 0.001f);
        } else {
            EXPECT_FLOAT_EQ(activations[i], 0.0f);
        }
    }
}

TEST_F(RegularizationTest, Dropout_NoDropInInference) {
    nimcp_dropout_config_t config = nimcp_dropout_default_config(0.5f);
    config.training = false;

    dropout_ctx = nimcp_dropout_create(&config);
    ASSERT_NE(dropout_ctx, nullptr);

    std::vector<float> activations = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    std::vector<float> original = activations;

    nimcp_dropout_forward(dropout_ctx, activations.data(), activations.size(), nullptr);

    // In inference mode, no dropout should happen
    for (size_t i = 0; i < activations.size(); i++) {
        EXPECT_FLOAT_EQ(activations[i], original[i]);
    }
}

TEST_F(RegularizationTest, Dropout_SetTrainingMode) {
    nimcp_dropout_config_t config = nimcp_dropout_default_config(0.5f);
    config.training = true;
    config.seed = 123;

    dropout_ctx = nimcp_dropout_create(&config);
    ASSERT_NE(dropout_ctx, nullptr);

    // Switch to inference mode
    nimcp_dropout_set_training(dropout_ctx, false);

    std::vector<float> activations = {1.0f, 2.0f, 3.0f};
    std::vector<float> original = activations;

    nimcp_dropout_forward(dropout_ctx, activations.data(), activations.size(), nullptr);

    for (size_t i = 0; i < activations.size(); i++) {
        EXPECT_FLOAT_EQ(activations[i], original[i]);
    }
}

TEST_F(RegularizationTest, Dropout_SetRate) {
    nimcp_dropout_config_t config = nimcp_dropout_default_config(0.3f);
    dropout_ctx = nimcp_dropout_create(&config);
    ASSERT_NE(dropout_ctx, nullptr);

    EXPECT_FLOAT_EQ(nimcp_dropout_get_rate(dropout_ctx), 0.3f);

    nimcp_result_t result = nimcp_dropout_set_rate(dropout_ctx, 0.6f);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_FLOAT_EQ(nimcp_dropout_get_rate(dropout_ctx), 0.6f);

    // Invalid rate
    result = nimcp_dropout_set_rate(dropout_ctx, 1.0f);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(RegularizationTest, Dropout_BackwardWithMask) {
    nimcp_dropout_config_t config = nimcp_dropout_default_config(0.5f);
    config.seed = 99;
    config.training = true;

    dropout_ctx = nimcp_dropout_create(&config);
    ASSERT_NE(dropout_ctx, nullptr);

    std::vector<float> activations(10, 1.0f);
    std::vector<uint8_t> mask(10);

    nimcp_dropout_forward(dropout_ctx, activations.data(), activations.size(), mask.data());

    // Create gradients
    std::vector<float> gradients(10, 1.0f);

    nimcp_dropout_backward(dropout_ctx, gradients.data(), gradients.size(), mask.data());

    // Gradients should be 0 where mask is 0, scaled elsewhere
    float scale = 2.0f;  // 1/(1-0.5)
    for (size_t i = 0; i < gradients.size(); i++) {
        if (mask[i]) {
            EXPECT_NEAR(gradients[i], scale, 0.001f);
        } else {
            EXPECT_FLOAT_EQ(gradients[i], 0.0f);
        }
    }
}

/* ============================================================================
 * Label Smoothing Tests
 * ============================================================================ */

TEST_F(RegularizationTest, LabelSmooth_SmoothsSingleLabel) {
    uint32_t num_classes = 10;
    std::vector<float> output(num_classes);

    nimcp_label_smooth_single(3, num_classes, 0.1f, output.data());

    float smooth_val = 0.1f / 10.0f;  // 0.01
    float confident_val = 1.0f - 0.1f + smooth_val;  // 0.91

    EXPECT_NEAR(output[3], confident_val, 1e-6f);

    for (uint32_t i = 0; i < num_classes; i++) {
        if (i != 3) {
            EXPECT_NEAR(output[i], smooth_val, 1e-6f);
        }
    }

    // Sum should be 1
    float sum = std::accumulate(output.begin(), output.end(), 0.0f);
    EXPECT_NEAR(sum, 1.0f, 1e-5f);
}

TEST_F(RegularizationTest, LabelSmooth_BatchProcessing) {
    uint32_t num_classes = 4;
    uint32_t num_samples = 3;

    // One-hot labels: [1,0,0,0], [0,1,0,0], [0,0,0,1]
    std::vector<float> labels = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };

    nimcp_label_smooth(labels.data(), num_samples, num_classes, 0.2f);

    float smooth_val = 0.2f / 4.0f;  // 0.05
    float confident_val = 1.0f - 0.2f + smooth_val;  // 0.85

    // Check first sample
    EXPECT_NEAR(labels[0], confident_val, 1e-6f);
    EXPECT_NEAR(labels[1], smooth_val, 1e-6f);
    EXPECT_NEAR(labels[2], smooth_val, 1e-6f);
    EXPECT_NEAR(labels[3], smooth_val, 1e-6f);

    // Check second sample
    EXPECT_NEAR(labels[4], smooth_val, 1e-6f);
    EXPECT_NEAR(labels[5], confident_val, 1e-6f);

    // Check third sample
    EXPECT_NEAR(labels[11], confident_val, 1e-6f);
}

/* ============================================================================
 * Early Stopping Tests
 * ============================================================================ */

TEST_F(RegularizationTest, EarlyStop_Creation) {
    nimcp_early_stop_config_t config = nimcp_early_stop_default_config(10);

    early_stop_ctx = nimcp_early_stop_create(&config);
    ASSERT_NE(early_stop_ctx, nullptr);
}

TEST_F(RegularizationTest, EarlyStop_StopsAfterPatience) {
    nimcp_early_stop_config_t config = nimcp_early_stop_default_config(3);
    config.mode = NIMCP_EARLY_STOP_MIN;
    config.min_delta = 0.001f;

    early_stop_ctx = nimcp_early_stop_create(&config);
    ASSERT_NE(early_stop_ctx, nullptr);

    // Improving metrics
    EXPECT_FALSE(nimcp_early_stop_check(early_stop_ctx, 1.0f));
    EXPECT_TRUE(nimcp_early_stop_improved(early_stop_ctx));

    EXPECT_FALSE(nimcp_early_stop_check(early_stop_ctx, 0.9f));
    EXPECT_TRUE(nimcp_early_stop_improved(early_stop_ctx));

    EXPECT_FALSE(nimcp_early_stop_check(early_stop_ctx, 0.8f));
    EXPECT_TRUE(nimcp_early_stop_improved(early_stop_ctx));

    // No improvement - patience = 3
    EXPECT_FALSE(nimcp_early_stop_check(early_stop_ctx, 0.85f));  // wait=1
    EXPECT_FALSE(nimcp_early_stop_improved(early_stop_ctx));

    EXPECT_FALSE(nimcp_early_stop_check(early_stop_ctx, 0.85f));  // wait=2

    // On 3rd no-improvement epoch, patience exhausted, should stop
    EXPECT_TRUE(nimcp_early_stop_check(early_stop_ctx, 0.85f));   // wait=3, stop!
}

TEST_F(RegularizationTest, EarlyStop_MaxMode) {
    nimcp_early_stop_config_t config;
    config.patience = 2;
    config.mode = NIMCP_EARLY_STOP_MAX;  // For accuracy
    config.min_delta = 0.01f;
    config.restore_best = true;

    early_stop_ctx = nimcp_early_stop_create(&config);
    ASSERT_NE(early_stop_ctx, nullptr);

    EXPECT_FALSE(nimcp_early_stop_check(early_stop_ctx, 0.5f));
    EXPECT_TRUE(nimcp_early_stop_improved(early_stop_ctx));

    EXPECT_FALSE(nimcp_early_stop_check(early_stop_ctx, 0.6f));  // Improved
    EXPECT_TRUE(nimcp_early_stop_improved(early_stop_ctx));

    EXPECT_FALSE(nimcp_early_stop_check(early_stop_ctx, 0.55f));  // Worse, wait=1
    EXPECT_FALSE(nimcp_early_stop_improved(early_stop_ctx));

    // On 2nd no-improvement epoch (patience=2), should stop
    EXPECT_TRUE(nimcp_early_stop_check(early_stop_ctx, 0.55f));   // wait=2, stop!

    EXPECT_FLOAT_EQ(nimcp_early_stop_get_best(early_stop_ctx), 0.6f);
    EXPECT_EQ(nimcp_early_stop_get_best_epoch(early_stop_ctx), 2u);
}

TEST_F(RegularizationTest, EarlyStop_Reset) {
    nimcp_early_stop_config_t config = nimcp_early_stop_default_config(5);
    early_stop_ctx = nimcp_early_stop_create(&config);
    ASSERT_NE(early_stop_ctx, nullptr);

    // Progress through some epochs
    nimcp_early_stop_check(early_stop_ctx, 1.0f);
    nimcp_early_stop_check(early_stop_ctx, 0.9f);
    nimcp_early_stop_check(early_stop_ctx, 0.8f);

    nimcp_early_stop_reset(early_stop_ctx);

    // After reset, should start fresh
    EXPECT_FALSE(nimcp_early_stop_check(early_stop_ctx, 1.5f));
    EXPECT_FLOAT_EQ(nimcp_early_stop_get_best(early_stop_ctx), 1.5f);
    EXPECT_EQ(nimcp_early_stop_get_best_epoch(early_stop_ctx), 1u);
}

/* ============================================================================
 * Statistics Tests
 * ============================================================================ */

TEST_F(RegularizationTest, Stats_TracksRegularizationLoss) {
    nimcp_regularization_config_t config = nimcp_regularization_default_config();
    config.weight_reg_type = NIMCP_REG_L2;
    config.weight_reg.l2.lambda = 0.01f;

    ctx = nimcp_regularization_create(&config);
    ASSERT_NE(ctx, nullptr);

    std::vector<float> weights = {1.0f, 2.0f, 3.0f};

    nimcp_regularization_loss(ctx, weights.data(), weights.size());
    nimcp_regularization_loss(ctx, weights.data(), weights.size());
    nimcp_regularization_loss(ctx, weights.data(), weights.size());

    nimcp_regularization_stats_t stats;
    nimcp_regularization_get_stats(ctx, &stats);

    EXPECT_EQ(stats.weight_reg_count, 3u);
    EXPECT_GT(stats.total_reg_loss, 0.0f);
}

TEST_F(RegularizationTest, Stats_TracksClipping) {
    nimcp_regularization_config_t config = nimcp_regularization_default_config();
    config.gradient_clip.mode = NIMCP_CLIP_BY_VALUE;
    config.gradient_clip.threshold = 1.0f;

    ctx = nimcp_regularization_create(&config);
    ASSERT_NE(ctx, nullptr);

    std::vector<float> gradients = {2.0f, -3.0f, 0.5f};
    nimcp_gradient_clip(ctx, gradients.data(), gradients.size());

    nimcp_regularization_stats_t stats;
    nimcp_regularization_get_stats(ctx, &stats);

    EXPECT_EQ(stats.clip_count, 1u);  // At least one clip event
}

TEST_F(RegularizationTest, Stats_Reset) {
    nimcp_regularization_config_t config = nimcp_regularization_default_config();
    config.weight_reg_type = NIMCP_REG_L2;
    config.weight_reg.l2.lambda = 0.01f;

    ctx = nimcp_regularization_create(&config);
    ASSERT_NE(ctx, nullptr);

    std::vector<float> weights = {1.0f, 2.0f, 3.0f};
    nimcp_regularization_loss(ctx, weights.data(), weights.size());

    nimcp_regularization_reset_stats(ctx);

    nimcp_regularization_stats_t stats;
    nimcp_regularization_get_stats(ctx, &stats);

    EXPECT_EQ(stats.weight_reg_count, 0u);
    EXPECT_FLOAT_EQ(stats.total_reg_loss, 0.0f);
}

/* ============================================================================
 * Utility Function Tests
 * ============================================================================ */

TEST_F(RegularizationTest, TypeNames) {
    EXPECT_STREQ(nimcp_reg_type_name(NIMCP_REG_NONE), "None");
    EXPECT_STREQ(nimcp_reg_type_name(NIMCP_REG_L1), "L1 (Lasso)");
    EXPECT_STREQ(nimcp_reg_type_name(NIMCP_REG_L2), "L2 (Ridge)");
    EXPECT_STREQ(nimcp_reg_type_name(NIMCP_REG_ELASTIC_NET), "Elastic Net");
}

TEST_F(RegularizationTest, ClipModeNames) {
    EXPECT_STREQ(nimcp_clip_mode_name(NIMCP_CLIP_NONE), "None");
    EXPECT_STREQ(nimcp_clip_mode_name(NIMCP_CLIP_BY_VALUE), "Clip by Value");
    EXPECT_STREQ(nimcp_clip_mode_name(NIMCP_CLIP_BY_NORM), "Clip by Norm");
    EXPECT_STREQ(nimcp_clip_mode_name(NIMCP_CLIP_BY_GLOBAL_NORM), "Clip by Global Norm");
}

/* ============================================================================
 * Validation Tests
 * ============================================================================ */

TEST_F(RegularizationTest, Validation_ValidConfig) {
    nimcp_regularization_config_t config = nimcp_regularization_default_config();
    config.weight_reg_type = NIMCP_REG_L2;
    config.weight_reg.l2.lambda = 0.01f;

    nimcp_result_t result = nimcp_regularization_validate_config(&config);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(RegularizationTest, Validation_InvalidLambda) {
    nimcp_regularization_config_t config = nimcp_regularization_default_config();
    config.weight_reg_type = NIMCP_REG_L2;
    config.weight_reg.l2.lambda = -0.01f;

    nimcp_result_t result = nimcp_regularization_validate_config(&config);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(RegularizationTest, Validation_InvalidElasticNetAlpha) {
    nimcp_regularization_config_t config = nimcp_regularization_default_config();
    config.weight_reg_type = NIMCP_REG_ELASTIC_NET;
    config.weight_reg.elastic_net.lambda = 0.01f;
    config.weight_reg.elastic_net.alpha = 1.5f;  // Invalid

    nimcp_result_t result = nimcp_regularization_validate_config(&config);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(RegularizationTest, Validation_InvalidClipThreshold) {
    nimcp_regularization_config_t config = nimcp_regularization_default_config();
    config.gradient_clip.mode = NIMCP_CLIP_BY_NORM;
    config.gradient_clip.threshold = -1.0f;

    nimcp_result_t result = nimcp_regularization_validate_config(&config);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(RegularizationTest, Validation_InvalidDropoutRate) {
    nimcp_regularization_config_t config = nimcp_regularization_default_config();
    config.dropout.rate = 1.0f;  // Invalid (must be < 1)

    nimcp_result_t result = nimcp_regularization_validate_config(&config);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

/* ============================================================================
 * Default Configuration Tests
 * ============================================================================ */

TEST_F(RegularizationTest, DefaultConfigs) {
    nimcp_l1_config_t l1 = nimcp_l1_default_config(0.05f);
    EXPECT_FLOAT_EQ(l1.lambda, 0.05f);

    nimcp_l2_config_t l2 = nimcp_l2_default_config(0.001f);
    EXPECT_FLOAT_EQ(l2.lambda, 0.001f);

    nimcp_elastic_net_config_t elastic = nimcp_elastic_net_default_config(0.01f, 0.7f);
    EXPECT_FLOAT_EQ(elastic.lambda, 0.01f);
    EXPECT_FLOAT_EQ(elastic.alpha, 0.7f);

    nimcp_dropout_config_t dropout = nimcp_dropout_default_config(0.25f);
    EXPECT_FLOAT_EQ(dropout.rate, 0.25f);
    EXPECT_EQ(dropout.mode, NIMCP_DROPOUT_STANDARD);
    EXPECT_TRUE(dropout.training);

    nimcp_clip_config_t clip = nimcp_clip_default_config(NIMCP_CLIP_BY_NORM, 5.0f);
    EXPECT_EQ(clip.mode, NIMCP_CLIP_BY_NORM);
    EXPECT_FLOAT_EQ(clip.threshold, 5.0f);

    nimcp_label_smooth_config_t smooth = nimcp_label_smooth_default_config(0.15f, 100);
    EXPECT_FLOAT_EQ(smooth.smoothing, 0.15f);
    EXPECT_EQ(smooth.num_classes, 100u);

    nimcp_early_stop_config_t early = nimcp_early_stop_default_config(20);
    EXPECT_EQ(early.patience, 20u);
    EXPECT_EQ(early.mode, NIMCP_EARLY_STOP_MIN);
}

/* ============================================================================
 * Edge Case Tests
 * ============================================================================ */

TEST_F(RegularizationTest, NullContext_HandledGracefully) {
    EXPECT_FLOAT_EQ(nimcp_l1_loss(nullptr, 0, 0.1f), 0.0f);
    EXPECT_FLOAT_EQ(nimcp_regularization_loss(nullptr, nullptr, 0), 0.0f);
    EXPECT_FLOAT_EQ(nimcp_gradient_clip(nullptr, nullptr, 0), 1.0f);
    EXPECT_FLOAT_EQ(nimcp_dropout_get_rate(nullptr), 0.0f);
    EXPECT_FALSE(nimcp_early_stop_check(nullptr, 1.0f));

    nimcp_regularization_stats_t stats;
    EXPECT_EQ(nimcp_regularization_get_stats(nullptr, &stats), NIMCP_ERROR_INVALID_PARAM);

    // Should not crash
    nimcp_regularization_destroy(nullptr);
    nimcp_dropout_destroy(nullptr);
    nimcp_early_stop_destroy(nullptr);
}

TEST_F(RegularizationTest, EmptyArrays_HandledGracefully) {
    std::vector<float> empty;

    EXPECT_FLOAT_EQ(nimcp_l1_loss(empty.data(), 0, 0.1f), 0.0f);
    EXPECT_FLOAT_EQ(nimcp_l2_loss(empty.data(), 0, 0.1f), 0.0f);
    EXPECT_FLOAT_EQ(nimcp_gradient_norm(empty.data(), 0), 0.0f);
    EXPECT_FLOAT_EQ(nimcp_gradient_clip_by_norm(empty.data(), 0, 1.0f), 1.0f);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
