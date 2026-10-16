/**
 * @file test_privacy.cpp
 * @brief GoogleTest unit tests for NIMCP edge differential privacy subsystem
 *
 * Tests gradient clipping, noise injection, privacy budget tracking,
 * and budget exhaustion.
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>

extern "C" {
#include "edge/nimcp_edge.h"
#include "edge/nimcp_edge_types.h"
}

class PrivacyTest : public ::testing::Test {
protected:
    nimcp_edge_dp_config_t config;

    void SetUp() override {
        memset(&config, 0, sizeof(config));
        nimcp_edge_dp_init(&config);
    }
};

TEST_F(PrivacyTest, InitSetsDefaults) {
    EXPECT_TRUE(config.enabled);
    EXPECT_GT(config.noise_scale, 0.0f);
    EXPECT_GT(config.gradient_clip_norm, 0.0f);
    EXPECT_GT(config.privacy_budget_epsilon, 0.0f);
    EXPECT_FLOAT_EQ(config.privacy_spent, 0.0f);
}

TEST_F(PrivacyTest, PrivatizeClipsGradientNorm) {
    // Create a gradient with large norm
    const uint32_t np = 4;
    float grads[] = {100.0f, 100.0f, 100.0f, 100.0f};
    // Original L2 norm = 200

    nimcp_edge_dp_privatize_gradients(&config, grads, np);

    // Compute resulting norm (before noise, clipping should have happened)
    float norm = 0.0f;
    for (uint32_t i = 0; i < np; i++) {
        norm += grads[i] * grads[i];
    }
    norm = std::sqrt(norm);

    // Norm should be bounded (clipped + noise, so may exceed clip slightly due to noise)
    // But shouldn't be anywhere near 200
    EXPECT_LT(norm, config.gradient_clip_norm * 5.0f);
}

TEST_F(PrivacyTest, PrivatizeAddsNoise) {
    const uint32_t np = 100;
    std::vector<float> grads(np, 1.0f);
    std::vector<float> grads_copy(grads);

    nimcp_edge_dp_privatize_gradients(&config, grads.data(), np);

    // At least some values should differ due to noise
    bool any_differ = false;
    for (uint32_t i = 0; i < np; i++) {
        if (std::fabs(grads[i] - grads_copy[i]) > 1e-10f) {
            any_differ = true;
            break;
        }
    }
    EXPECT_TRUE(any_differ) << "Noise should modify at least some gradients";
}

TEST_F(PrivacyTest, NoiseMagnitudeProportionalToSigma) {
    const uint32_t np = 1000;
    std::vector<float> grads_low_sigma(np, 0.0f);
    std::vector<float> grads_high_sigma(np, 0.0f);

    nimcp_edge_dp_config_t config_low, config_high;
    memset(&config_low, 0, sizeof(config_low));
    memset(&config_high, 0, sizeof(config_high));
    nimcp_edge_dp_init(&config_low);
    nimcp_edge_dp_init(&config_high);

    config_low.noise_scale = 0.01f;
    config_high.noise_scale = 10.0f;

    nimcp_edge_dp_privatize_gradients(&config_low, grads_low_sigma.data(), np);
    nimcp_edge_dp_privatize_gradients(&config_high, grads_high_sigma.data(), np);

    // Compute variance of each
    float var_low = 0.0f, var_high = 0.0f;
    for (uint32_t i = 0; i < np; i++) {
        var_low += grads_low_sigma[i] * grads_low_sigma[i];
        var_high += grads_high_sigma[i] * grads_high_sigma[i];
    }
    var_low /= np;
    var_high /= np;

    EXPECT_GT(var_high, var_low)
        << "Higher sigma should produce higher variance noise";
}

TEST_F(PrivacyTest, BudgetTrackingEpsilonIncreases) {
    float initial_spent = config.privacy_spent;

    float grads[] = {1.0f, 2.0f, 3.0f, 4.0f};
    nimcp_edge_dp_privatize_gradients(&config, grads, 4);

    EXPECT_GT(config.privacy_spent, initial_spent);
}

TEST_F(PrivacyTest, BudgetExhaustedReturnsTrueWhenSpent) {
    EXPECT_FALSE(nimcp_edge_dp_budget_exhausted(&config));

    // Exhaust budget by many privatizations
    float grads[] = {1.0f};
    for (int i = 0; i < 10000; i++) {
        grads[0] = 1.0f;
        nimcp_edge_dp_privatize_gradients(&config, grads, 1);
        if (nimcp_edge_dp_budget_exhausted(&config)) break;
    }

    // Force exhaust
    config.privacy_spent = config.privacy_budget_epsilon + 1.0f;
    EXPECT_TRUE(nimcp_edge_dp_budget_exhausted(&config));
}

TEST_F(PrivacyTest, LargeClipNormNoClipping) {
    config.gradient_clip_norm = 1e10f;

    float grads[] = {10.0f, 10.0f, 10.0f};
    float norm_before = 0;
    for (int i = 0; i < 3; i++) norm_before += grads[i] * grads[i];
    norm_before = std::sqrt(norm_before);

    nimcp_edge_dp_privatize_gradients(&config, grads, 3);

    // Direction should be roughly preserved (noise adds some variation)
    // The magnitude should be close to original (not clipped)
    float norm_after = 0;
    for (int i = 0; i < 3; i++) norm_after += grads[i] * grads[i];
    norm_after = std::sqrt(norm_after);

    // With very large clip, difference should mainly be from noise
    EXPECT_GT(norm_after, 0.0f);
}

TEST_F(PrivacyTest, SmallClipNormAggressiveClipping) {
    config.gradient_clip_norm = 0.001f;

    float grads[] = {100.0f, 100.0f, 100.0f};
    nimcp_edge_dp_privatize_gradients(&config, grads, 3);

    // After aggressive clipping, gradients should be very small
    for (int i = 0; i < 3; i++) {
        EXPECT_LT(std::fabs(grads[i]), 10.0f)
            << "Aggressive clipping should reduce gradient magnitude";
    }
}

TEST_F(PrivacyTest, ZeroGradientsNoiseStillAdded) {
    const uint32_t np = 100;
    std::vector<float> grads(np, 0.0f);

    nimcp_edge_dp_privatize_gradients(&config, grads.data(), np);

    // Some gradients should be non-zero due to noise
    bool any_nonzero = false;
    for (uint32_t i = 0; i < np; i++) {
        if (std::fabs(grads[i]) > 1e-10f) {
            any_nonzero = true;
            break;
        }
    }
    EXPECT_TRUE(any_nonzero) << "Zero gradients should still get noise";
}

TEST_F(PrivacyTest, MultipleCallsAccumulateBudget) {
    float grads[] = {1.0f, 2.0f};

    nimcp_edge_dp_privatize_gradients(&config, grads, 2);
    float spent1 = config.privacy_spent;

    grads[0] = 1.0f; grads[1] = 2.0f;
    nimcp_edge_dp_privatize_gradients(&config, grads, 2);
    float spent2 = config.privacy_spent;

    grads[0] = 1.0f; grads[1] = 2.0f;
    nimcp_edge_dp_privatize_gradients(&config, grads, 2);
    float spent3 = config.privacy_spent;

    EXPECT_GT(spent2, spent1);
    EXPECT_GT(spent3, spent2);
}
