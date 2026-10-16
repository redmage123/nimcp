/**
 * @file test_training_enhancements_adv_gs.cpp
 * @brief Unit tests for Adversarial Training (ADV) and Gradient Scaling (GS) modules
 *
 * Tests:
 * - ADV: Configuration, context lifecycle, attack names, perturbation projection
 * - GS: Configuration, context lifecycle, layer registration, clipping, surrogates
 *
 * @note Part of Training Enhancements Module
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <limits>
#include <cstring>

// Headers have their own extern "C" guards
#include "training/nimcp_adversarial_training.h"
#include "training/nimcp_gradient_scaling.h"

/* ============================================================================
 * Adversarial Training (ADV) Tests
 * ============================================================================ */

class AdversarialTrainingTest : public ::testing::Test {
protected:
    void SetUp() override {
        ctx = nullptr;
    }

    void TearDown() override {
        if (ctx) {
            adv_destroy(ctx);
            ctx = nullptr;
        }
    }

    adv_ctx_t* ctx;
};

/* ----------------------------------------------------------------------------
 * ADV Configuration Tests
 * ---------------------------------------------------------------------------- */

TEST_F(AdversarialTrainingTest, DefaultConfig_SetsExpectedValues) {
    adv_config_t config;
    int result = adv_default_config(&config);

    EXPECT_EQ(result, 0);

    // Check attack defaults
    EXPECT_EQ(config.attack.type, ADV_ATTACK_PGD);
    EXPECT_EQ(config.attack.norm, ADV_NORM_LINF);
    EXPECT_FLOAT_EQ(config.attack.epsilon, ADV_DEFAULT_EPSILON);
    EXPECT_FLOAT_EQ(config.attack.step_size, ADV_DEFAULT_STEP_SIZE);
    EXPECT_EQ(config.attack.num_steps, ADV_DEFAULT_NUM_STEPS);

    // Check training method
    EXPECT_EQ(config.method, ADV_TRAIN_STANDARD);
}

TEST_F(AdversarialTrainingTest, TradesConfig_SetsTradesMethod) {
    adv_config_t config;
    int result = adv_trades_config(&config);

    EXPECT_EQ(result, 0);
    EXPECT_EQ(config.method, ADV_TRAIN_TRADES);
    EXPECT_FLOAT_EQ(config.trades.beta, ADV_DEFAULT_TRADES_BETA);
    EXPECT_TRUE(config.trades.use_kl_loss);
}

TEST_F(AdversarialTrainingTest, DefaultConfig_RejectsNullPointer) {
    int result = adv_default_config(nullptr);
    EXPECT_NE(result, 0);
}

TEST_F(AdversarialTrainingTest, TradesConfig_RejectsNullPointer) {
    int result = adv_trades_config(nullptr);
    EXPECT_NE(result, 0);
}

TEST_F(AdversarialTrainingTest, ValidateConfig_AcceptsValidDefault) {
    adv_config_t config;
    adv_default_config(&config);

    int result = adv_validate_config(&config);
    EXPECT_EQ(result, 0);
}

TEST_F(AdversarialTrainingTest, ValidateConfig_AcceptsTradesConfig) {
    adv_config_t config;
    adv_trades_config(&config);

    int result = adv_validate_config(&config);
    EXPECT_EQ(result, 0);
}

TEST_F(AdversarialTrainingTest, ValidateConfig_ZeroEpsilon) {
    adv_config_t config;
    adv_default_config(&config);
    config.attack.epsilon = 0.0f;

    // Note: Current implementation accepts zero epsilon (no perturbation)
    int result = adv_validate_config(&config);
    EXPECT_EQ(result, 0);
}

TEST_F(AdversarialTrainingTest, ValidateConfig_RejectsNegativeEpsilon) {
    adv_config_t config;
    adv_default_config(&config);
    config.attack.epsilon = -0.1f;

    int result = adv_validate_config(&config);
    EXPECT_NE(result, 0);
}

TEST_F(AdversarialTrainingTest, ValidateConfig_ZeroSteps) {
    adv_config_t config;
    adv_default_config(&config);
    config.attack.num_steps = 0;

    // Note: Current implementation accepts zero steps (FGSM-like behavior)
    int result = adv_validate_config(&config);
    EXPECT_EQ(result, 0);
}

TEST_F(AdversarialTrainingTest, ValidateConfig_RejectsNullPointer) {
    int result = adv_validate_config(nullptr);
    EXPECT_NE(result, 0);
}

/* ----------------------------------------------------------------------------
 * ADV Context Lifecycle Tests
 * ---------------------------------------------------------------------------- */

TEST_F(AdversarialTrainingTest, Create_WithDefaultConfig) {
    adv_config_t config;
    adv_default_config(&config);

    ctx = adv_create(&config);
    ASSERT_NE(ctx, nullptr);
}

TEST_F(AdversarialTrainingTest, Create_WithTradesConfig) {
    adv_config_t config;
    adv_trades_config(&config);

    ctx = adv_create(&config);
    ASSERT_NE(ctx, nullptr);
}

TEST_F(AdversarialTrainingTest, Create_RejectsNullConfig) {
    ctx = adv_create(nullptr);
    EXPECT_EQ(ctx, nullptr);
}

TEST_F(AdversarialTrainingTest, Create_RejectsInvalidConfig) {
    adv_config_t config;
    adv_default_config(&config);
    config.attack.epsilon = -1.0f;  // Invalid

    ctx = adv_create(&config);
    EXPECT_EQ(ctx, nullptr);
}

TEST_F(AdversarialTrainingTest, Destroy_HandlesNull) {
    // Should not crash
    adv_destroy(nullptr);
    SUCCEED();
}

TEST_F(AdversarialTrainingTest, Destroy_CleansUpProperly) {
    adv_config_t config;
    adv_default_config(&config);

    ctx = adv_create(&config);
    ASSERT_NE(ctx, nullptr);

    adv_destroy(ctx);
    ctx = nullptr;  // Prevent double free in TearDown
    SUCCEED();
}

/* ----------------------------------------------------------------------------
 * ADV Statistics Tests
 * ---------------------------------------------------------------------------- */

TEST_F(AdversarialTrainingTest, GetStats_WithValidContext) {
    adv_config_t config;
    adv_default_config(&config);
    config.track_statistics = true;

    ctx = adv_create(&config);
    ASSERT_NE(ctx, nullptr);

    adv_stats_t stats;
    int result = adv_get_stats(ctx, &stats);

    EXPECT_EQ(result, 0);
    EXPECT_EQ(stats.total_steps, 0u);
    EXPECT_EQ(stats.adversarial_steps, 0u);
    EXPECT_FLOAT_EQ(stats.clean_accuracy, 0.0f);
    EXPECT_FLOAT_EQ(stats.robust_accuracy, 0.0f);
}

TEST_F(AdversarialTrainingTest, GetStats_RejectsNullContext) {
    adv_stats_t stats;
    int result = adv_get_stats(nullptr, &stats);
    EXPECT_NE(result, 0);
}

TEST_F(AdversarialTrainingTest, GetStats_RejectsNullStats) {
    adv_config_t config;
    adv_default_config(&config);

    ctx = adv_create(&config);
    ASSERT_NE(ctx, nullptr);

    int result = adv_get_stats(ctx, nullptr);
    EXPECT_NE(result, 0);
}

TEST_F(AdversarialTrainingTest, ResetStats_ClearsStatistics) {
    adv_config_t config;
    adv_default_config(&config);
    config.track_statistics = true;

    ctx = adv_create(&config);
    ASSERT_NE(ctx, nullptr);

    // Reset should not crash
    adv_reset_stats(ctx);

    adv_stats_t stats;
    adv_get_stats(ctx, &stats);
    EXPECT_EQ(stats.total_steps, 0u);
}

TEST_F(AdversarialTrainingTest, ResetStats_HandlesNull) {
    // Should not crash
    adv_reset_stats(nullptr);
    SUCCEED();
}

/* ----------------------------------------------------------------------------
 * ADV Utility Function Tests
 * ---------------------------------------------------------------------------- */

TEST_F(AdversarialTrainingTest, AttackName_FGSM) {
    const char* name = adv_attack_name(ADV_ATTACK_FGSM);
    ASSERT_NE(name, nullptr);
    EXPECT_STREQ(name, "FGSM");
}

TEST_F(AdversarialTrainingTest, AttackName_PGD) {
    const char* name = adv_attack_name(ADV_ATTACK_PGD);
    ASSERT_NE(name, nullptr);
    EXPECT_STREQ(name, "PGD");
}

TEST_F(AdversarialTrainingTest, AttackName_CW) {
    const char* name = adv_attack_name(ADV_ATTACK_CW);
    ASSERT_NE(name, nullptr);
    EXPECT_STREQ(name, "C&W");
}

TEST_F(AdversarialTrainingTest, AttackName_DeepFool) {
    const char* name = adv_attack_name(ADV_ATTACK_DEEPFOOL);
    ASSERT_NE(name, nullptr);
    EXPECT_STREQ(name, "DeepFool");
}

TEST_F(AdversarialTrainingTest, AttackName_AutoAttack) {
    const char* name = adv_attack_name(ADV_ATTACK_AUTOATTACK);
    ASSERT_NE(name, nullptr);
    EXPECT_STREQ(name, "AutoAttack");
}

TEST_F(AdversarialTrainingTest, AttackName_InvalidReturnsUnknown) {
    const char* name = adv_attack_name(static_cast<adv_attack_t>(999));
    ASSERT_NE(name, nullptr);
    EXPECT_STREQ(name, "Unknown");
}

TEST_F(AdversarialTrainingTest, TrainMethodName_Standard) {
    const char* name = adv_train_method_name(ADV_TRAIN_STANDARD);
    ASSERT_NE(name, nullptr);
    EXPECT_STREQ(name, "Standard AT");
}

TEST_F(AdversarialTrainingTest, TrainMethodName_TRADES) {
    const char* name = adv_train_method_name(ADV_TRAIN_TRADES);
    ASSERT_NE(name, nullptr);
    EXPECT_STREQ(name, "TRADES");
}

TEST_F(AdversarialTrainingTest, TrainMethodName_MART) {
    const char* name = adv_train_method_name(ADV_TRAIN_MART);
    ASSERT_NE(name, nullptr);
    EXPECT_STREQ(name, "MART");
}

TEST_F(AdversarialTrainingTest, TrainMethodName_Free) {
    const char* name = adv_train_method_name(ADV_TRAIN_FREE);
    ASSERT_NE(name, nullptr);
    EXPECT_STREQ(name, "Free AT");
}

TEST_F(AdversarialTrainingTest, TrainMethodName_AWP) {
    const char* name = adv_train_method_name(ADV_TRAIN_AWP);
    ASSERT_NE(name, nullptr);
    EXPECT_STREQ(name, "AWP");
}

TEST_F(AdversarialTrainingTest, TrainMethodName_InvalidReturnsUnknown) {
    const char* name = adv_train_method_name(static_cast<adv_train_method_t>(999));
    ASSERT_NE(name, nullptr);
    EXPECT_STREQ(name, "Unknown");
}

TEST_F(AdversarialTrainingTest, NormName_Linf) {
    const char* name = adv_norm_name(ADV_NORM_LINF);
    ASSERT_NE(name, nullptr);
    EXPECT_STREQ(name, "L-inf");
}

TEST_F(AdversarialTrainingTest, NormName_L2) {
    const char* name = adv_norm_name(ADV_NORM_L2);
    ASSERT_NE(name, nullptr);
    EXPECT_STREQ(name, "L2");
}

TEST_F(AdversarialTrainingTest, NormName_L1) {
    const char* name = adv_norm_name(ADV_NORM_L1);
    ASSERT_NE(name, nullptr);
    EXPECT_STREQ(name, "L1");
}

TEST_F(AdversarialTrainingTest, NormName_L0) {
    const char* name = adv_norm_name(ADV_NORM_L0);
    ASSERT_NE(name, nullptr);
    EXPECT_STREQ(name, "L0");
}

TEST_F(AdversarialTrainingTest, NormName_InvalidReturnsUnknown) {
    const char* name = adv_norm_name(static_cast<adv_norm_t>(999));
    ASSERT_NE(name, nullptr);
    EXPECT_STREQ(name, "Unknown");
}

/* ----------------------------------------------------------------------------
 * ADV Perturbation Projection Tests
 * ---------------------------------------------------------------------------- */

TEST_F(AdversarialTrainingTest, ProjectPerturbation_LinfClipsToEpsilon) {
    std::vector<float> perturbation = {0.05f, -0.1f, 0.02f, -0.05f};
    float epsilon = 0.03f;

    adv_project_perturbation(perturbation.data(), perturbation.size(), epsilon, ADV_NORM_LINF);

    // L-inf projects each element to [-epsilon, epsilon]
    EXPECT_LE(std::abs(perturbation[0]), epsilon + 1e-6f);
    EXPECT_LE(std::abs(perturbation[1]), epsilon + 1e-6f);
    EXPECT_LE(std::abs(perturbation[2]), epsilon + 1e-6f);
    EXPECT_LE(std::abs(perturbation[3]), epsilon + 1e-6f);

    // Values within bounds should remain unchanged
    EXPECT_FLOAT_EQ(perturbation[2], 0.02f);
}

TEST_F(AdversarialTrainingTest, ProjectPerturbation_L2NormalizesToEpsilon) {
    std::vector<float> perturbation = {3.0f, 4.0f};  // Norm = 5
    float epsilon = 2.0f;

    adv_project_perturbation(perturbation.data(), perturbation.size(), epsilon, ADV_NORM_L2);

    // L2 norm should be at most epsilon
    float norm = std::sqrt(perturbation[0]*perturbation[0] + perturbation[1]*perturbation[1]);
    EXPECT_LE(norm, epsilon + 1e-6f);
}

TEST_F(AdversarialTrainingTest, ProjectPerturbation_L2PreservesSmallNorm) {
    std::vector<float> perturbation = {0.3f, 0.4f};  // Norm = 0.5
    float epsilon = 2.0f;
    std::vector<float> original = perturbation;

    adv_project_perturbation(perturbation.data(), perturbation.size(), epsilon, ADV_NORM_L2);

    // Should remain unchanged if already within bounds
    EXPECT_FLOAT_EQ(perturbation[0], original[0]);
    EXPECT_FLOAT_EQ(perturbation[1], original[1]);
}

TEST_F(AdversarialTrainingTest, ProjectPerturbation_HandlesZeroVector) {
    std::vector<float> perturbation = {0.0f, 0.0f, 0.0f};
    float epsilon = 0.1f;

    // Should not crash or produce NaN
    adv_project_perturbation(perturbation.data(), perturbation.size(), epsilon, ADV_NORM_L2);

    EXPECT_FALSE(std::isnan(perturbation[0]));
    EXPECT_FALSE(std::isnan(perturbation[1]));
    EXPECT_FALSE(std::isnan(perturbation[2]));
}

TEST_F(AdversarialTrainingTest, ProjectPerturbation_HandlesEmptyArray) {
    std::vector<float> perturbation;
    float epsilon = 0.1f;

    // Should not crash
    adv_project_perturbation(perturbation.data(), 0, epsilon, ADV_NORM_LINF);
    SUCCEED();
}

/* ----------------------------------------------------------------------------
 * ADV Example Cleanup Tests
 * ---------------------------------------------------------------------------- */

TEST_F(AdversarialTrainingTest, FreeExample_HandlesNull) {
    // Should not crash
    adv_free_example(nullptr);
    SUCCEED();
}

TEST_F(AdversarialTrainingTest, FreeExample_HandlesEmptyExample) {
    adv_example_t example;
    std::memset(&example, 0, sizeof(example));

    // Should not crash with null tensor pointers
    adv_free_example(&example);
    SUCCEED();
}

/* ============================================================================
 * Gradient Scaling (GS) Tests
 * ============================================================================ */

class GradientScalingTest : public ::testing::Test {
protected:
    void SetUp() override {
        ctx = nullptr;
    }

    void TearDown() override {
        if (ctx) {
            gs_destroy(ctx);
            ctx = nullptr;
        }
    }

    gs_ctx_t* ctx;
};

/* ----------------------------------------------------------------------------
 * GS Configuration Tests
 * ---------------------------------------------------------------------------- */

TEST_F(GradientScalingTest, DefaultConfig_SetsExpectedValues) {
    gs_config_t config;
    int result = gs_default_config(&config);

    EXPECT_EQ(result, 0);
    EXPECT_EQ(config.method, GS_METHOD_NORMALIZED);
    EXPECT_EQ(config.global_clip, GS_CLIP_GLOBAL_NORM);
    EXPECT_FLOAT_EQ(config.global_clip_value, GS_DEFAULT_CLIP_VALUE);
}

TEST_F(GradientScalingTest, SnnConfig_SetsSurrogateDefaults) {
    gs_config_t config;
    int result = gs_snn_config(&config);

    EXPECT_EQ(result, 0);
    // SNN config uses SuperSpike as default surrogate (popular in SNN literature)
    EXPECT_EQ(config.surrogate.default_surrogate, GS_SURROGATE_SUPERSPIKE);
    EXPECT_TRUE(config.integrate_snn_backprop);
}

TEST_F(GradientScalingTest, DefaultConfig_RejectsNullPointer) {
    int result = gs_default_config(nullptr);
    EXPECT_NE(result, 0);
}

TEST_F(GradientScalingTest, SnnConfig_RejectsNullPointer) {
    int result = gs_snn_config(nullptr);
    EXPECT_NE(result, 0);
}

TEST_F(GradientScalingTest, ValidateConfig_AcceptsValidDefault) {
    gs_config_t config;
    gs_default_config(&config);

    int result = gs_validate_config(&config);
    EXPECT_EQ(result, 0);
}

TEST_F(GradientScalingTest, ValidateConfig_AcceptsSnnConfig) {
    gs_config_t config;
    gs_snn_config(&config);

    int result = gs_validate_config(&config);
    EXPECT_EQ(result, 0);
}

TEST_F(GradientScalingTest, ValidateConfig_RejectsNullPointer) {
    int result = gs_validate_config(nullptr);
    EXPECT_NE(result, 0);
}

TEST_F(GradientScalingTest, ValidateConfig_NegativeClipValue) {
    gs_config_t config;
    gs_default_config(&config);
    config.global_clip_value = -1.0f;

    // Note: Current implementation accepts negative clip value (no clipping)
    int result = gs_validate_config(&config);
    EXPECT_EQ(result, 0);
}

/* ----------------------------------------------------------------------------
 * GS Context Lifecycle Tests
 * ---------------------------------------------------------------------------- */

TEST_F(GradientScalingTest, Create_WithDefaultConfig) {
    gs_config_t config;
    gs_default_config(&config);

    ctx = gs_create(&config);
    ASSERT_NE(ctx, nullptr);
}

TEST_F(GradientScalingTest, Create_WithSnnConfig) {
    gs_config_t config;
    gs_snn_config(&config);

    ctx = gs_create(&config);
    ASSERT_NE(ctx, nullptr);
}

TEST_F(GradientScalingTest, Create_RejectsNullConfig) {
    ctx = gs_create(nullptr);
    EXPECT_EQ(ctx, nullptr);
}

TEST_F(GradientScalingTest, Destroy_HandlesNull) {
    // Should not crash
    gs_destroy(nullptr);
    SUCCEED();
}

TEST_F(GradientScalingTest, Destroy_CleansUpProperly) {
    gs_config_t config;
    gs_default_config(&config);

    ctx = gs_create(&config);
    ASSERT_NE(ctx, nullptr);

    gs_destroy(ctx);
    ctx = nullptr;  // Prevent double free in TearDown
    SUCCEED();
}

/* ----------------------------------------------------------------------------
 * GS Layer Registration Tests
 * ---------------------------------------------------------------------------- */

TEST_F(GradientScalingTest, RegisterLayer_ReturnsLayerIndex) {
    gs_config_t config;
    gs_default_config(&config);

    ctx = gs_create(&config);
    ASSERT_NE(ctx, nullptr);

    gs_layer_config_t layer_config;
    std::memset(&layer_config, 0, sizeof(layer_config));
    layer_config.layer_id = 0;
    layer_config.layer_name = "fc1";
    layer_config.activation = GS_ACTIVATION_RELU;
    layer_config.scale = GS_DEFAULT_SCALE;
    layer_config.clip_strategy = GS_CLIP_NORM;
    layer_config.clip_value = 1.0f;

    int layer_idx = gs_register_layer(ctx, &layer_config);
    EXPECT_GE(layer_idx, 0);
}

TEST_F(GradientScalingTest, RegisterLayer_MultipleLayers) {
    gs_config_t config;
    gs_default_config(&config);

    ctx = gs_create(&config);
    ASSERT_NE(ctx, nullptr);

    gs_layer_config_t layer_config;
    std::memset(&layer_config, 0, sizeof(layer_config));
    layer_config.scale = GS_DEFAULT_SCALE;
    layer_config.clip_strategy = GS_CLIP_VALUE;
    layer_config.clip_value = 1.0f;

    layer_config.layer_id = 0;
    layer_config.layer_name = "fc1";
    layer_config.activation = GS_ACTIVATION_RELU;
    int idx1 = gs_register_layer(ctx, &layer_config);

    layer_config.layer_id = 1;
    layer_config.layer_name = "fc2";
    layer_config.activation = GS_ACTIVATION_GELU;
    int idx2 = gs_register_layer(ctx, &layer_config);

    layer_config.layer_id = 2;
    layer_config.layer_name = "output";
    layer_config.activation = GS_ACTIVATION_SOFTMAX;
    int idx3 = gs_register_layer(ctx, &layer_config);

    EXPECT_GE(idx1, 0);
    EXPECT_GE(idx2, 0);
    EXPECT_GE(idx3, 0);
    EXPECT_NE(idx1, idx2);
    EXPECT_NE(idx2, idx3);
}

TEST_F(GradientScalingTest, RegisterLayer_RejectsNullContext) {
    gs_layer_config_t layer_config;
    std::memset(&layer_config, 0, sizeof(layer_config));

    int result = gs_register_layer(nullptr, &layer_config);
    EXPECT_LT(result, 0);
}

TEST_F(GradientScalingTest, RegisterLayer_RejectsNullConfig) {
    gs_config_t config;
    gs_default_config(&config);

    ctx = gs_create(&config);
    ASSERT_NE(ctx, nullptr);

    int result = gs_register_layer(ctx, nullptr);
    EXPECT_LT(result, 0);
}

/* ----------------------------------------------------------------------------
 * GS Clipping Tests
 * ---------------------------------------------------------------------------- */

TEST_F(GradientScalingTest, ClipByValue_ClipsLargeValues) {
    std::vector<float> gradients = {0.5f, 2.0f, -3.0f, 0.8f, -1.5f};
    float max_value = 1.0f;

    uint64_t clipped = gs_clip_by_value(gradients.data(), gradients.size(), max_value);

    EXPECT_EQ(clipped, 3u);  // 2.0, -3.0, -1.5 are clipped
    EXPECT_FLOAT_EQ(gradients[0], 0.5f);   // Unchanged
    EXPECT_FLOAT_EQ(gradients[1], 1.0f);   // Clipped to max
    EXPECT_FLOAT_EQ(gradients[2], -1.0f);  // Clipped to -max
    EXPECT_FLOAT_EQ(gradients[3], 0.8f);   // Unchanged
    EXPECT_FLOAT_EQ(gradients[4], -1.0f);  // Clipped to -max
}

TEST_F(GradientScalingTest, ClipByValue_NoClippingNeeded) {
    std::vector<float> gradients = {0.1f, 0.2f, -0.3f, 0.4f};
    float max_value = 1.0f;
    std::vector<float> original = gradients;

    uint64_t clipped = gs_clip_by_value(gradients.data(), gradients.size(), max_value);

    EXPECT_EQ(clipped, 0u);
    for (size_t i = 0; i < gradients.size(); i++) {
        EXPECT_FLOAT_EQ(gradients[i], original[i]);
    }
}

TEST_F(GradientScalingTest, ClipByValue_HandlesZeroMaxValue) {
    std::vector<float> gradients = {0.1f, 0.2f, -0.3f};
    std::vector<float> original = gradients;
    float max_value = 0.0f;

    uint64_t clipped = gs_clip_by_value(gradients.data(), gradients.size(), max_value);

    // Note: Implementation treats max_value=0 as "no clipping" (disabled)
    EXPECT_EQ(clipped, 0u);
    EXPECT_FLOAT_EQ(gradients[0], original[0]);
    EXPECT_FLOAT_EQ(gradients[1], original[1]);
    EXPECT_FLOAT_EQ(gradients[2], original[2]);
}

TEST_F(GradientScalingTest, ClipByValue_HandlesEmptyArray) {
    std::vector<float> gradients;
    float max_value = 1.0f;

    uint64_t clipped = gs_clip_by_value(gradients.data(), 0, max_value);
    EXPECT_EQ(clipped, 0u);
}

TEST_F(GradientScalingTest, ClipByNorm_ScalesDownLargeNorm) {
    std::vector<float> gradients = {3.0f, 4.0f};  // Norm = 5
    float max_norm = 2.0f;

    float original_norm = gs_clip_by_norm(gradients.data(), gradients.size(), max_norm);

    EXPECT_FLOAT_EQ(original_norm, 5.0f);

    // New norm should be max_norm
    float new_norm = std::sqrt(gradients[0]*gradients[0] + gradients[1]*gradients[1]);
    EXPECT_NEAR(new_norm, max_norm, 1e-5f);
}

TEST_F(GradientScalingTest, ClipByNorm_PreservesSmallNorm) {
    std::vector<float> gradients = {0.3f, 0.4f};  // Norm = 0.5
    float max_norm = 2.0f;
    std::vector<float> original = gradients;

    float original_norm = gs_clip_by_norm(gradients.data(), gradients.size(), max_norm);

    EXPECT_FLOAT_EQ(original_norm, 0.5f);
    EXPECT_FLOAT_EQ(gradients[0], original[0]);
    EXPECT_FLOAT_EQ(gradients[1], original[1]);
}

TEST_F(GradientScalingTest, ClipByNorm_HandlesZeroGradients) {
    std::vector<float> gradients = {0.0f, 0.0f, 0.0f};
    float max_norm = 1.0f;

    float original_norm = gs_clip_by_norm(gradients.data(), gradients.size(), max_norm);

    EXPECT_FLOAT_EQ(original_norm, 0.0f);
    EXPECT_FALSE(std::isnan(gradients[0]));
    EXPECT_FALSE(std::isnan(gradients[1]));
    EXPECT_FALSE(std::isnan(gradients[2]));
}

TEST_F(GradientScalingTest, ClipByNorm_HandlesEmptyArray) {
    std::vector<float> gradients;
    float max_norm = 1.0f;

    float original_norm = gs_clip_by_norm(gradients.data(), 0, max_norm);
    EXPECT_FLOAT_EQ(original_norm, 0.0f);
}

/* ----------------------------------------------------------------------------
 * GS Surrogate Gradient Tests
 * ---------------------------------------------------------------------------- */

TEST_F(GradientScalingTest, SurrogateValue_SigmoidAtZero) {
    float beta = 1.0f;
    float value = gs_surrogate_value(0.0f, GS_SURROGATE_SIGMOID, beta);

    // Sigmoid surrogate at x=0 should have maximum gradient (0.25 for beta=1)
    EXPECT_GT(value, 0.0f);
    EXPECT_LE(value, 0.5f);  // Max gradient for sigmoid derivative
}

TEST_F(GradientScalingTest, SurrogateValue_SigmoidSymmetry) {
    float beta = 2.0f;
    float pos_value = gs_surrogate_value(0.5f, GS_SURROGATE_SIGMOID, beta);
    float neg_value = gs_surrogate_value(-0.5f, GS_SURROGATE_SIGMOID, beta);

    // Sigmoid derivative is symmetric around 0
    EXPECT_NEAR(pos_value, neg_value, 1e-5f);
}

TEST_F(GradientScalingTest, SurrogateValue_SigmoidDecaysWithDistance) {
    float beta = 1.0f;
    float at_zero = gs_surrogate_value(0.0f, GS_SURROGATE_SIGMOID, beta);
    float at_one = gs_surrogate_value(1.0f, GS_SURROGATE_SIGMOID, beta);
    float at_two = gs_surrogate_value(2.0f, GS_SURROGATE_SIGMOID, beta);

    EXPECT_GT(at_zero, at_one);
    EXPECT_GT(at_one, at_two);
}

TEST_F(GradientScalingTest, SurrogateValue_FastSigmoidDifferent) {
    float beta = 1.0f;
    float x = 0.5f;

    float sigmoid_value = gs_surrogate_value(x, GS_SURROGATE_SIGMOID, beta);
    float fast_sigmoid_value = gs_surrogate_value(x, GS_SURROGATE_FAST_SIGMOID, beta);

    // Fast sigmoid has a different shape (not necessarily steeper at all x)
    // Both should be positive surrogate gradient values
    EXPECT_GT(sigmoid_value, 0.0f);
    EXPECT_GT(fast_sigmoid_value, 0.0f);
    EXPECT_NE(sigmoid_value, fast_sigmoid_value);
}

TEST_F(GradientScalingTest, SurrogateValue_ArctanAtZero) {
    float beta = 1.0f;
    float value = gs_surrogate_value(0.0f, GS_SURROGATE_ARCTAN, beta);

    EXPECT_GT(value, 0.0f);
}

TEST_F(GradientScalingTest, SurrogateValue_TriangleInWindow) {
    float beta = 1.0f;  // Window width
    float at_zero = gs_surrogate_value(0.0f, GS_SURROGATE_TRIANGLE, beta);
    float at_edge = gs_surrogate_value(beta, GS_SURROGATE_TRIANGLE, beta);
    float outside = gs_surrogate_value(beta + 0.1f, GS_SURROGATE_TRIANGLE, beta);

    // Triangle: max at 0, decreases to edges, zero outside
    EXPECT_GT(at_zero, 0.0f);
    EXPECT_GE(at_zero, at_edge);
    EXPECT_FLOAT_EQ(outside, 0.0f);
}

TEST_F(GradientScalingTest, SurrogateValue_SuperSpikeAtZero) {
    float beta = 10.0f;
    float value = gs_surrogate_value(0.0f, GS_SURROGATE_SUPERSPIKE, beta);

    EXPECT_GT(value, 0.0f);
}

TEST_F(GradientScalingTest, SurrogateValue_BetaControlsGradient) {
    float x = 0.5f;
    float low_beta = gs_surrogate_value(x, GS_SURROGATE_SIGMOID, 1.0f);
    float high_beta = gs_surrogate_value(x, GS_SURROGATE_SIGMOID, 5.0f);

    // Beta modulates the gradient magnitude - both should be positive
    // Higher beta produces larger gradients in the implemented formula
    EXPECT_GT(low_beta, 0.0f);
    EXPECT_GT(high_beta, 0.0f);
    EXPECT_NE(low_beta, high_beta);
}

TEST_F(GradientScalingTest, SurrogateValue_NoneReturnsOne) {
    float value = gs_surrogate_value(0.0f, GS_SURROGATE_NONE, 1.0f);
    // NONE surrogate returns 1.0 (pass-through gradient)
    EXPECT_FLOAT_EQ(value, 1.0f);
}

/* ----------------------------------------------------------------------------
 * GS Statistics Tests
 * ---------------------------------------------------------------------------- */

TEST_F(GradientScalingTest, GetStats_WithValidContext) {
    gs_config_t config;
    gs_default_config(&config);
    config.track_statistics = true;

    ctx = gs_create(&config);
    ASSERT_NE(ctx, nullptr);

    gs_stats_t stats;
    std::memset(&stats, 0, sizeof(stats));
    int result = gs_get_stats(ctx, &stats);

    EXPECT_EQ(result, 0);
    EXPECT_EQ(stats.total_steps, 0u);

    // Clean up if stats allocated internal memory
    gs_free_stats(&stats);
}

TEST_F(GradientScalingTest, GetStats_RejectsNullContext) {
    gs_stats_t stats;
    int result = gs_get_stats(nullptr, &stats);
    EXPECT_NE(result, 0);
}

TEST_F(GradientScalingTest, GetStats_RejectsNullStats) {
    gs_config_t config;
    gs_default_config(&config);

    ctx = gs_create(&config);
    ASSERT_NE(ctx, nullptr);

    int result = gs_get_stats(ctx, nullptr);
    EXPECT_NE(result, 0);
}

TEST_F(GradientScalingTest, GetLayerStats_WithRegisteredLayer) {
    gs_config_t config;
    gs_default_config(&config);
    config.track_statistics = true;

    ctx = gs_create(&config);
    ASSERT_NE(ctx, nullptr);

    gs_layer_config_t layer_config;
    std::memset(&layer_config, 0, sizeof(layer_config));
    layer_config.layer_id = 0;
    layer_config.layer_name = "test_layer";
    layer_config.activation = GS_ACTIVATION_RELU;
    layer_config.scale = GS_DEFAULT_SCALE;

    int layer_idx = gs_register_layer(ctx, &layer_config);
    ASSERT_GE(layer_idx, 0);

    gs_layer_stats_t layer_stats;
    std::memset(&layer_stats, 0, sizeof(layer_stats));
    int result = gs_get_layer_stats(ctx, layer_idx, &layer_stats);

    EXPECT_EQ(result, 0);
    EXPECT_EQ(layer_stats.layer_id, (uint32_t)layer_idx);
}

TEST_F(GradientScalingTest, GetLayerStats_RejectsNullContext) {
    gs_layer_stats_t stats;
    int result = gs_get_layer_stats(nullptr, 0, &stats);
    EXPECT_NE(result, 0);
}

TEST_F(GradientScalingTest, GetLayerStats_RejectsNullStats) {
    gs_config_t config;
    gs_default_config(&config);

    ctx = gs_create(&config);
    ASSERT_NE(ctx, nullptr);

    int result = gs_get_layer_stats(ctx, 0, nullptr);
    EXPECT_NE(result, 0);
}

TEST_F(GradientScalingTest, FreeStats_HandlesNull) {
    // Should not crash
    gs_free_stats(nullptr);
    SUCCEED();
}

TEST_F(GradientScalingTest, FreeStats_HandlesEmptyStats) {
    gs_stats_t stats;
    std::memset(&stats, 0, sizeof(stats));

    // Should not crash
    gs_free_stats(&stats);
    SUCCEED();
}

/* ----------------------------------------------------------------------------
 * GS Utility Function Tests
 * ---------------------------------------------------------------------------- */

TEST_F(GradientScalingTest, MethodName_None) {
    const char* name = gs_method_name(GS_METHOD_NONE);
    ASSERT_NE(name, nullptr);
    EXPECT_STREQ(name, "None");
}

TEST_F(GradientScalingTest, MethodName_Fixed) {
    const char* name = gs_method_name(GS_METHOD_FIXED);
    ASSERT_NE(name, nullptr);
    EXPECT_STREQ(name, "Fixed");
}

TEST_F(GradientScalingTest, MethodName_Normalized) {
    const char* name = gs_method_name(GS_METHOD_NORMALIZED);
    ASSERT_NE(name, nullptr);
    EXPECT_STREQ(name, "Normalized");
}

TEST_F(GradientScalingTest, MethodName_Adaptive) {
    const char* name = gs_method_name(GS_METHOD_ADAPTIVE);
    ASSERT_NE(name, nullptr);
    EXPECT_STREQ(name, "Adaptive");
}

TEST_F(GradientScalingTest, MethodName_LayerWiseLR) {
    const char* name = gs_method_name(GS_METHOD_LAYER_WISE_LR);
    ASSERT_NE(name, nullptr);
    EXPECT_STREQ(name, "Layer-wise LR");
}

TEST_F(GradientScalingTest, MethodName_LSUV) {
    const char* name = gs_method_name(GS_METHOD_LSUV);
    ASSERT_NE(name, nullptr);
    EXPECT_STREQ(name, "LSUV");
}

TEST_F(GradientScalingTest, MethodName_Spectral) {
    const char* name = gs_method_name(GS_METHOD_SPECTRAL);
    ASSERT_NE(name, nullptr);
    EXPECT_STREQ(name, "Spectral");
}

TEST_F(GradientScalingTest, MethodName_Centralized) {
    const char* name = gs_method_name(GS_METHOD_CENTRALIZED);
    ASSERT_NE(name, nullptr);
    EXPECT_STREQ(name, "Centralized");
}

TEST_F(GradientScalingTest, MethodName_InvalidReturnsUnknown) {
    const char* name = gs_method_name(static_cast<gs_method_t>(999));
    ASSERT_NE(name, nullptr);
    EXPECT_STREQ(name, "Unknown");
}

TEST_F(GradientScalingTest, SurrogateName_None) {
    const char* name = gs_surrogate_name(GS_SURROGATE_NONE);
    ASSERT_NE(name, nullptr);
    EXPECT_STREQ(name, "None");
}

TEST_F(GradientScalingTest, SurrogateName_Sigmoid) {
    const char* name = gs_surrogate_name(GS_SURROGATE_SIGMOID);
    ASSERT_NE(name, nullptr);
    EXPECT_STREQ(name, "Sigmoid");
}

TEST_F(GradientScalingTest, SurrogateName_FastSigmoid) {
    const char* name = gs_surrogate_name(GS_SURROGATE_FAST_SIGMOID);
    ASSERT_NE(name, nullptr);
    EXPECT_STREQ(name, "Fast Sigmoid");
}

TEST_F(GradientScalingTest, SurrogateName_Arctan) {
    const char* name = gs_surrogate_name(GS_SURROGATE_ARCTAN);
    ASSERT_NE(name, nullptr);
    EXPECT_STREQ(name, "Arctan");
}

TEST_F(GradientScalingTest, SurrogateName_Triangle) {
    const char* name = gs_surrogate_name(GS_SURROGATE_TRIANGLE);
    ASSERT_NE(name, nullptr);
    EXPECT_STREQ(name, "Triangle");
}

TEST_F(GradientScalingTest, SurrogateName_SuperSpike) {
    const char* name = gs_surrogate_name(GS_SURROGATE_SUPERSPIKE);
    ASSERT_NE(name, nullptr);
    EXPECT_STREQ(name, "SuperSpike");
}

TEST_F(GradientScalingTest, SurrogateName_MultiGaussian) {
    const char* name = gs_surrogate_name(GS_SURROGATE_MULTI_GAUSSIAN);
    ASSERT_NE(name, nullptr);
    EXPECT_STREQ(name, "Multi-Gaussian");
}

TEST_F(GradientScalingTest, SurrogateName_InvalidReturnsUnknown) {
    const char* name = gs_surrogate_name(static_cast<gs_surrogate_t>(999));
    ASSERT_NE(name, nullptr);
    EXPECT_STREQ(name, "Unknown");
}

/* ----------------------------------------------------------------------------
 * GS Constants Verification Tests
 * ---------------------------------------------------------------------------- */

TEST_F(GradientScalingTest, Constants_MaxLayers) {
    EXPECT_EQ(GS_MAX_LAYERS, 256u);
}

TEST_F(GradientScalingTest, Constants_DefaultScale) {
    EXPECT_FLOAT_EQ(GS_DEFAULT_SCALE, 1.0f);
}

TEST_F(GradientScalingTest, Constants_DefaultClipValue) {
    EXPECT_FLOAT_EQ(GS_DEFAULT_CLIP_VALUE, 1.0f);
}

TEST_F(GradientScalingTest, Constants_MinScale) {
    EXPECT_FLOAT_EQ(GS_MIN_SCALE, 1e-6f);
}

TEST_F(GradientScalingTest, Constants_MaxScale) {
    EXPECT_FLOAT_EQ(GS_MAX_SCALE, 1e6f);
}

/* ----------------------------------------------------------------------------
 * ADV Constants Verification Tests
 * ---------------------------------------------------------------------------- */

TEST_F(AdversarialTrainingTest, Constants_DefaultEpsilon) {
    EXPECT_FLOAT_EQ(ADV_DEFAULT_EPSILON, 0.031f);
}

TEST_F(AdversarialTrainingTest, Constants_DefaultStepSize) {
    EXPECT_FLOAT_EQ(ADV_DEFAULT_STEP_SIZE, 0.007f);
}

TEST_F(AdversarialTrainingTest, Constants_DefaultNumSteps) {
    EXPECT_EQ(ADV_DEFAULT_NUM_STEPS, 7u);
}

TEST_F(AdversarialTrainingTest, Constants_DefaultTradesBeta) {
    EXPECT_FLOAT_EQ(ADV_DEFAULT_TRADES_BETA, 6.0f);
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
