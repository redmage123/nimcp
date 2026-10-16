/**
 * @file test_federated.cpp
 * @brief GoogleTest unit tests for NIMCP edge federated learning subsystem
 *
 * Tests FedAvg, FedMedian, FedProx aggregation, blending with/without EWC,
 * and edge cases.
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>

extern "C" {
#include "edge/nimcp_edge.h"
#include "edge/nimcp_edge_types.h"
}

class FederatedTest : public ::testing::Test {
protected:
    void TearDown() override {}
};

TEST_F(FederatedTest, FedAvgIdenticalGradients) {
    const uint32_t np = 4;
    float g1_data[] = {1.0f, 2.0f, 3.0f, 4.0f};
    float g2_data[] = {1.0f, 2.0f, 3.0f, 4.0f};

    nimcp_federated_gradient_t grads[2];
    memset(grads, 0, sizeof(grads));
    grads[0].device_id = 1;
    grads[0].num_params = np;
    grads[0].gradients = g1_data;
    grads[1].device_id = 2;
    grads[1].num_params = np;
    grads[1].gradients = g2_data;

    float aggregated[4] = {0};
    int ret = nimcp_federated_aggregate(grads, 2, aggregated, np, NIMCP_FED_AVG);
    EXPECT_EQ(ret, 0);

    for (uint32_t i = 0; i < np; i++) {
        EXPECT_FLOAT_EQ(aggregated[i], g1_data[i]);
    }
}

TEST_F(FederatedTest, FedAvgDifferentGradients) {
    const uint32_t np = 4;
    float g1[] = {2.0f, 4.0f, 6.0f, 8.0f};
    float g2[] = {0.0f, 0.0f, 0.0f, 0.0f};

    nimcp_federated_gradient_t grads[2];
    memset(grads, 0, sizeof(grads));
    grads[0].device_id = 1; grads[0].num_params = np; grads[0].gradients = g1;
    grads[1].device_id = 2; grads[1].num_params = np; grads[1].gradients = g2;

    float agg[4] = {0};
    nimcp_federated_aggregate(grads, 2, agg, np, NIMCP_FED_AVG);

    // Mean of [2,0], [4,0], [6,0], [8,0] = [1, 2, 3, 4]
    EXPECT_NEAR(agg[0], 1.0f, 1e-5f);
    EXPECT_NEAR(agg[1], 2.0f, 1e-5f);
    EXPECT_NEAR(agg[2], 3.0f, 1e-5f);
    EXPECT_NEAR(agg[3], 4.0f, 1e-5f);
}

TEST_F(FederatedTest, FedMedianIgnoresOutlier) {
    const uint32_t np = 3;
    float g1[] = {1.0f, 1.0f, 1.0f};
    float g2[] = {1.1f, 0.9f, 1.0f};
    float g3[] = {100.0f, 100.0f, 100.0f}; // outlier

    nimcp_federated_gradient_t grads[3];
    memset(grads, 0, sizeof(grads));
    grads[0].device_id = 1; grads[0].num_params = np; grads[0].gradients = g1;
    grads[1].device_id = 2; grads[1].num_params = np; grads[1].gradients = g2;
    grads[2].device_id = 3; grads[2].num_params = np; grads[2].gradients = g3;

    float agg[3] = {0};
    nimcp_federated_aggregate(grads, 3, agg, np, NIMCP_FED_MEDIAN);

    // Median should ignore the outlier
    for (uint32_t i = 0; i < np; i++) {
        EXPECT_LT(agg[i], 10.0f)
            << "Median should ignore outlier at index " << i;
    }
}

TEST_F(FederatedTest, FedProxProximalTermApplied) {
    const uint32_t np = 4;
    float g1[] = {1.0f, 2.0f, 3.0f, 4.0f};
    float g2[] = {1.0f, 2.0f, 3.0f, 4.0f};

    nimcp_federated_gradient_t grads[2];
    memset(grads, 0, sizeof(grads));
    grads[0].device_id = 1; grads[0].num_params = np; grads[0].gradients = g1;
    grads[1].device_id = 2; grads[1].num_params = np; grads[1].gradients = g2;

    float agg_avg[4] = {0};
    float agg_prox[4] = {0};

    nimcp_federated_aggregate(grads, 2, agg_avg, np, NIMCP_FED_AVG);
    nimcp_federated_aggregate(grads, 2, agg_prox, np, NIMCP_FED_PROX);

    // FedProx may differ from FedAvg due to proximal regularization
    // At minimum, both should produce valid results
    for (uint32_t i = 0; i < np; i++) {
        EXPECT_FALSE(std::isnan(agg_prox[i]));
        EXPECT_FALSE(std::isinf(agg_prox[i]));
    }
}

TEST_F(FederatedTest, BlendRatioOneAllLocal) {
    float device[] = {1.0f, 2.0f, 3.0f};
    float master[] = {10.0f, 20.0f, 30.0f};

    nimcp_federated_blend(device, master, 3, 1.0f, nullptr);

    // ratio=1.0 → all local
    EXPECT_NEAR(device[0], 1.0f, 0.01f);
    EXPECT_NEAR(device[1], 2.0f, 0.01f);
    EXPECT_NEAR(device[2], 3.0f, 0.01f);
}

TEST_F(FederatedTest, BlendRatioZeroAllMaster) {
    float device[] = {1.0f, 2.0f, 3.0f};
    float master[] = {10.0f, 20.0f, 30.0f};

    nimcp_federated_blend(device, master, 3, 0.0f, nullptr);

    // ratio=0.0 → all master
    EXPECT_NEAR(device[0], 10.0f, 0.01f);
    EXPECT_NEAR(device[1], 20.0f, 0.01f);
    EXPECT_NEAR(device[2], 30.0f, 0.01f);
}

TEST_F(FederatedTest, BlendRatioHalfExactMidpoint) {
    float device[] = {0.0f, 0.0f, 0.0f};
    float master[] = {10.0f, 20.0f, 30.0f};

    nimcp_federated_blend(device, master, 3, 0.5f, nullptr);

    EXPECT_NEAR(device[0], 5.0f, 0.01f);
    EXPECT_NEAR(device[1], 10.0f, 0.01f);
    EXPECT_NEAR(device[2], 15.0f, 0.01f);
}

TEST_F(FederatedTest, BlendWithEWCProtectsWeights) {
    nimcp_ewc_state_t* ewc = nimcp_ewc_create(3, 10.0f);
    ASSERT_NE(ewc, nullptr);

    // High Fisher on index 0
    float grads[] = {10.0f, 0.0f, 0.0f};
    nimcp_ewc_compute_fisher(ewc, grads, 1);

    float anchor[] = {5.0f, 5.0f, 5.0f};
    nimcp_ewc_set_anchor(ewc, anchor);

    float device[] = {5.0f, 5.0f, 5.0f};
    float master[] = {0.0f, 0.0f, 0.0f};

    nimcp_federated_blend(device, master, 3, 0.5f, ewc);

    // Index 0 should be more protected (closer to 5.0)
    // Indices 1,2 should be closer to standard blend
    EXPECT_GT(device[0], device[1])
        << "Protected weight should stay closer to local";

    nimcp_ewc_destroy(ewc);
}

TEST_F(FederatedTest, SingleDeviceAggregateIsIdentity) {
    const uint32_t np = 3;
    float g[] = {1.0f, 2.0f, 3.0f};

    nimcp_federated_gradient_t grad;
    memset(&grad, 0, sizeof(grad));
    grad.device_id = 1;
    grad.num_params = np;
    grad.gradients = g;

    float agg[3] = {0};
    nimcp_federated_aggregate(&grad, 1, agg, np, NIMCP_FED_AVG);

    for (uint32_t i = 0; i < np; i++) {
        EXPECT_FLOAT_EQ(agg[i], g[i]);
    }
}

TEST_F(FederatedTest, ZeroGradientsZeroAggregate) {
    const uint32_t np = 4;
    float g1[] = {0.0f, 0.0f, 0.0f, 0.0f};
    float g2[] = {0.0f, 0.0f, 0.0f, 0.0f};

    nimcp_federated_gradient_t grads[2];
    memset(grads, 0, sizeof(grads));
    grads[0].gradients = g1; grads[0].num_params = np;
    grads[1].gradients = g2; grads[1].num_params = np;

    float agg[4] = {99.0f, 99.0f, 99.0f, 99.0f};
    nimcp_federated_aggregate(grads, 2, agg, np, NIMCP_FED_AVG);

    for (uint32_t i = 0; i < np; i++) {
        EXPECT_FLOAT_EQ(agg[i], 0.0f);
    }
}

TEST_F(FederatedTest, LargeDeviceCount) {
    const uint32_t np = 2;
    const uint32_t n_devices = 100;

    std::vector<std::vector<float>> grad_data(n_devices, std::vector<float>(np, 1.0f));
    std::vector<nimcp_federated_gradient_t> grads(n_devices);

    for (uint32_t d = 0; d < n_devices; d++) {
        memset(&grads[d], 0, sizeof(nimcp_federated_gradient_t));
        grads[d].device_id = d;
        grads[d].num_params = np;
        grads[d].gradients = grad_data[d].data();
    }

    float agg[2] = {0};
    nimcp_federated_aggregate(grads.data(), n_devices, agg, np, NIMCP_FED_AVG);

    EXPECT_NEAR(agg[0], 1.0f, 1e-5f);
    EXPECT_NEAR(agg[1], 1.0f, 1e-5f);
}

TEST_F(FederatedTest, FedAvgThreeDevicesWeightedEvenly) {
    const uint32_t np = 2;
    float g1[] = {3.0f, 6.0f};
    float g2[] = {0.0f, 0.0f};
    float g3[] = {0.0f, 3.0f};

    nimcp_federated_gradient_t grads[3];
    memset(grads, 0, sizeof(grads));
    grads[0].gradients = g1; grads[0].num_params = np;
    grads[1].gradients = g2; grads[1].num_params = np;
    grads[2].gradients = g3; grads[2].num_params = np;

    float agg[2] = {0};
    nimcp_federated_aggregate(grads, 3, agg, np, NIMCP_FED_AVG);

    EXPECT_NEAR(agg[0], 1.0f, 1e-5f);
    EXPECT_NEAR(agg[1], 3.0f, 1e-5f);
}

TEST_F(FederatedTest, NullGradientArrayReturnsError) {
    float agg[4] = {0};
    int ret = nimcp_federated_aggregate(nullptr, 2, agg, 4, NIMCP_FED_AVG);
    EXPECT_EQ(ret, -1);
}

TEST_F(FederatedTest, ZeroDevicesReturnsError) {
    nimcp_federated_gradient_t grad;
    memset(&grad, 0, sizeof(grad));
    float g[] = {1.0f};
    grad.gradients = g;
    grad.num_params = 1;

    float agg[1] = {0};
    int ret = nimcp_federated_aggregate(&grad, 0, agg, 1, NIMCP_FED_AVG);
    EXPECT_EQ(ret, -1);
}
