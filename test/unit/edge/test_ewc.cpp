/**
 * @file test_ewc.cpp
 * @brief GoogleTest unit tests for NIMCP edge Elastic Weight Consolidation
 *
 * Tests Fisher information computation, anchor weight setting,
 * EWC-aware blending, and lambda scaling.
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>

extern "C" {
#include "edge/nimcp_edge.h"
#include "edge/nimcp_edge_types.h"
}

class EWCTest : public ::testing::Test {
protected:
    nimcp_ewc_state_t* ewc = nullptr;

    void TearDown() override {
        if (ewc) {
            nimcp_ewc_destroy(ewc);
            ewc = nullptr;
        }
    }
};

TEST_F(EWCTest, CreateAllocatesCorrectly) {
    ewc = nimcp_ewc_create(100, 1.0f);
    ASSERT_NE(ewc, nullptr);
    EXPECT_EQ(ewc->num_params, 100u);
    EXPECT_FLOAT_EQ(ewc->ewc_lambda, 1.0f);
    EXPECT_NE(ewc->fisher_diagonal, nullptr);
    EXPECT_NE(ewc->anchor_weights, nullptr);
}

TEST_F(EWCTest, DestroyFreesMemory) {
    ewc = nimcp_ewc_create(50, 1.0f);
    ASSERT_NE(ewc, nullptr);
    nimcp_ewc_destroy(ewc);
    ewc = nullptr; // Prevent double-free
}

TEST_F(EWCTest, ComputeFisherUniformGradients) {
    ewc = nimcp_ewc_create(4, 1.0f);
    ASSERT_NE(ewc, nullptr);

    // Uniform gradients: all 1.0
    float gradients[] = {1.0f, 1.0f, 1.0f, 1.0f};
    int ret = nimcp_ewc_compute_fisher(ewc, gradients, 1);
    EXPECT_EQ(ret, 0);

    // Fisher should be uniform (squared gradients)
    for (uint32_t i = 0; i < 4; i++) {
        EXPECT_NEAR(ewc->fisher_diagonal[i], ewc->fisher_diagonal[0], 1e-6f)
            << "Fisher should be uniform at index " << i;
    }
}

TEST_F(EWCTest, ComputeFisherOneLargeGradient) {
    ewc = nimcp_ewc_create(4, 1.0f);
    ASSERT_NE(ewc, nullptr);

    // One large gradient, rest small
    float gradients[] = {10.0f, 0.1f, 0.1f, 0.1f};
    nimcp_ewc_compute_fisher(ewc, gradients, 1);

    // Fisher[0] should be much larger than others
    EXPECT_GT(ewc->fisher_diagonal[0], ewc->fisher_diagonal[1]);
    EXPECT_GT(ewc->fisher_diagonal[0], ewc->fisher_diagonal[2]);
    EXPECT_GT(ewc->fisher_diagonal[0], ewc->fisher_diagonal[3]);
}

TEST_F(EWCTest, SetAnchorStoresWeights) {
    ewc = nimcp_ewc_create(4, 1.0f);
    ASSERT_NE(ewc, nullptr);

    float weights[] = {0.5f, 1.5f, -0.3f, 2.1f};
    int ret = nimcp_ewc_set_anchor(ewc, weights);
    EXPECT_EQ(ret, 0);

    for (uint32_t i = 0; i < 4; i++) {
        EXPECT_FLOAT_EQ(ewc->anchor_weights[i], weights[i]);
    }
}

TEST_F(EWCTest, BlendHighFisherFavorsLocal) {
    ewc = nimcp_ewc_create(4, 10.0f); // High lambda
    ASSERT_NE(ewc, nullptr);

    // Set high Fisher importance for all weights
    float gradients[] = {10.0f, 10.0f, 10.0f, 10.0f};
    nimcp_ewc_compute_fisher(ewc, gradients, 1);

    float local[] = {1.0f, 1.0f, 1.0f, 1.0f};
    nimcp_ewc_set_anchor(ewc, local);

    float master[] = {0.0f, 0.0f, 0.0f, 0.0f};

    // Blend with 0.5 ratio
    nimcp_ewc_blend_weights(ewc, local, master, 0.5f);

    // With high Fisher + high lambda, local weights should be preserved more
    for (uint32_t i = 0; i < 4; i++) {
        EXPECT_GT(local[i], 0.5f)
            << "High Fisher should favor local at index " << i;
    }
}

TEST_F(EWCTest, BlendLowFisherFavorsMaster) {
    ewc = nimcp_ewc_create(4, 0.001f); // Very low lambda
    ASSERT_NE(ewc, nullptr);

    // Very low Fisher importance
    float gradients[] = {0.001f, 0.001f, 0.001f, 0.001f};
    nimcp_ewc_compute_fisher(ewc, gradients, 1);

    float local[] = {1.0f, 1.0f, 1.0f, 1.0f};
    nimcp_ewc_set_anchor(ewc, local);

    float master[] = {0.0f, 0.0f, 0.0f, 0.0f};

    nimcp_ewc_blend_weights(ewc, local, master, 0.5f);

    // Low Fisher + low lambda → should be close to standard blend (0.5)
    for (uint32_t i = 0; i < 4; i++) {
        EXPECT_LT(local[i], 0.8f)
            << "Low Fisher should allow master influence at index " << i;
    }
}

TEST_F(EWCTest, BlendZeroFisherUsesLowImportanceRatio) {
    ewc = nimcp_ewc_create(4, 1.0f);
    ASSERT_NE(ewc, nullptr);

    // Zero Fisher: no importance
    float gradients[] = {0.0f, 0.0f, 0.0f, 0.0f};
    nimcp_ewc_compute_fisher(ewc, gradients, 1);

    float local[] = {1.0f, 1.0f, 1.0f, 1.0f};
    nimcp_ewc_set_anchor(ewc, local);

    float master[] = {0.0f, 0.0f, 0.0f, 0.0f};

    // With zero Fisher, penalty = lambda * 0 * diff^2 = 0, which is <= threshold (1.0)
    // So it uses low-importance blend: 0.3 * local + 0.7 * master
    // Result: 0.3 * 1.0 + 0.7 * 0.0 = 0.3
    nimcp_ewc_blend_weights(ewc, local, master, 0.0f);

    for (uint32_t i = 0; i < 4; i++) {
        EXPECT_NEAR(local[i], 0.3f, 0.01f);
    }
}

TEST_F(EWCTest, LambdaScalingHigherMeansMoreProtection) {
    // Low lambda
    nimcp_ewc_state_t* ewc_low = nimcp_ewc_create(4, 0.1f);
    // High lambda
    nimcp_ewc_state_t* ewc_high = nimcp_ewc_create(4, 100.0f);
    ASSERT_NE(ewc_low, nullptr);
    ASSERT_NE(ewc_high, nullptr);

    float gradients[] = {5.0f, 5.0f, 5.0f, 5.0f};
    nimcp_ewc_compute_fisher(ewc_low, gradients, 1);
    nimcp_ewc_compute_fisher(ewc_high, gradients, 1);

    float anchor[] = {1.0f, 1.0f, 1.0f, 1.0f};
    nimcp_ewc_set_anchor(ewc_low, anchor);
    nimcp_ewc_set_anchor(ewc_high, anchor);

    float local_low[] = {1.0f, 1.0f, 1.0f, 1.0f};
    float local_high[] = {1.0f, 1.0f, 1.0f, 1.0f};
    float master[] = {0.0f, 0.0f, 0.0f, 0.0f};

    nimcp_ewc_blend_weights(ewc_low, local_low, master, 0.5f);
    nimcp_ewc_blend_weights(ewc_high, local_high, master, 0.5f);

    // Higher lambda should keep local weights closer to original
    for (uint32_t i = 0; i < 4; i++) {
        EXPECT_GE(local_high[i], local_low[i])
            << "Higher lambda should preserve more at index " << i;
    }

    nimcp_ewc_destroy(ewc_low);
    nimcp_ewc_destroy(ewc_high);
}

TEST_F(EWCTest, NullEWCInFederatedBlendUniformBlend) {
    float device[] = {1.0f, 1.0f, 1.0f, 1.0f};
    float master[] = {0.0f, 0.0f, 0.0f, 0.0f};

    // Null EWC should fall back to uniform blend
    int ret = nimcp_federated_blend(device, master, 4, 0.5f, nullptr);
    EXPECT_EQ(ret, 0);

    // With ratio=0.5, result should be midpoint
    for (uint32_t i = 0; i < 4; i++) {
        EXPECT_NEAR(device[i], 0.5f, 0.1f);
    }
}

TEST_F(EWCTest, AllZeroGradients) {
    ewc = nimcp_ewc_create(4, 1.0f);
    ASSERT_NE(ewc, nullptr);

    float gradients[] = {0.0f, 0.0f, 0.0f, 0.0f};
    int ret = nimcp_ewc_compute_fisher(ewc, gradients, 1);
    EXPECT_EQ(ret, 0);

    // All Fisher values should be zero
    for (uint32_t i = 0; i < 4; i++) {
        EXPECT_FLOAT_EQ(ewc->fisher_diagonal[i], 0.0f);
    }
}

TEST_F(EWCTest, DestroyNullSafe) {
    nimcp_ewc_destroy(nullptr);
}
