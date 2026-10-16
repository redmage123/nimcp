/**
 * @file test_gradient_validation.cpp
 * @brief Unit tests for federated gradient validation
 *
 * WHAT: Verify gradient validation (NaN/Inf/extreme magnitudes) in federated learning
 * WHY:  Poisoned gradients from compromised devices must be detected and neutralized
 * HOW:  Create gradient arrays with NaN/Inf/extreme values, verify aggregation handles them
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <vector>
#include <limits>

extern "C" {
#include "edge/nimcp_edge.h"
}

// ============================================================================
// Gradient Validation Test Fixture
// ============================================================================

class GradientValidationTest : public ::testing::Test {
protected:
    static const uint32_t NUM_PARAMS = 10;

    void SetUp() override {}
    void TearDown() override {}

    // Helper: create a gradient struct with given values
    nimcp_federated_gradient_t make_gradient(uint32_t device_id,
                                              float* grads,
                                              uint32_t num_params) {
        nimcp_federated_gradient_t g;
        memset(&g, 0, sizeof(g));
        g.device_id = device_id;
        g.num_params = num_params;
        g.gradients = grads;
        g.local_steps = 1;
        return g;
    }
};

// --- Basic Aggregation ---

TEST_F(GradientValidationTest, ValidGradientsPass) {
    float grads1[NUM_PARAMS], grads2[NUM_PARAMS], aggregated[NUM_PARAMS];

    for (uint32_t i = 0; i < NUM_PARAMS; i++) {
        grads1[i] = 1.0f;
        grads2[i] = 3.0f;
    }

    nimcp_federated_gradient_t devices[2] = {
        make_gradient(1, grads1, NUM_PARAMS),
        make_gradient(2, grads2, NUM_PARAMS),
    };

    int ret = nimcp_federated_aggregate(devices, 2, aggregated, NUM_PARAMS,
                                         NIMCP_FED_AVG);
    EXPECT_EQ(ret, 0);

    // Average of 1.0 and 3.0 = 2.0
    for (uint32_t i = 0; i < NUM_PARAMS; i++) {
        EXPECT_NEAR(aggregated[i], 2.0f, 1e-5f);
    }
}

TEST_F(GradientValidationTest, NaNGradientHandled) {
    // If a device has NaN gradients, the aggregate function should handle it.
    // The current federated_aggregate does straight averaging, so NaN propagates.
    // This test documents the behavior.
    float grads1[NUM_PARAMS], grads2[NUM_PARAMS], aggregated[NUM_PARAMS];

    for (uint32_t i = 0; i < NUM_PARAMS; i++) {
        grads1[i] = 1.0f;
        grads2[i] = 1.0f;
    }
    grads1[5] = std::numeric_limits<float>::quiet_NaN();

    nimcp_federated_gradient_t devices[2] = {
        make_gradient(1, grads1, NUM_PARAMS),
        make_gradient(2, grads2, NUM_PARAMS),
    };

    int ret = nimcp_federated_aggregate(devices, 2, aggregated, NUM_PARAMS,
                                         NIMCP_FED_AVG);
    EXPECT_EQ(ret, 0);

    // Poisoned device (with NaN) is zeroed before aggregation.
    // Result = (0.0 + 1.0) / 2 = 0.5 for non-NaN params.
    // NaN param: (0.0 + 1.0) / 2 = 0.5 (poisoned device zeroed entirely).
    EXPECT_FALSE(std::isnan(aggregated[5])); /* NaN should be sanitized */
    EXPECT_NEAR(aggregated[0], 0.5f, 1e-5f);
}

TEST_F(GradientValidationTest, InfGradientHandled) {
    float grads1[NUM_PARAMS], grads2[NUM_PARAMS], aggregated[NUM_PARAMS];

    for (uint32_t i = 0; i < NUM_PARAMS; i++) {
        grads1[i] = 1.0f;
        grads2[i] = 1.0f;
    }
    grads1[3] = std::numeric_limits<float>::infinity();

    nimcp_federated_gradient_t devices[2] = {
        make_gradient(1, grads1, NUM_PARAMS),
        make_gradient(2, grads2, NUM_PARAMS),
    };

    int ret = nimcp_federated_aggregate(devices, 2, aggregated, NUM_PARAMS,
                                         NIMCP_FED_AVG);
    EXPECT_EQ(ret, 0);

    // Poisoned device (Inf) is zeroed. Result = (0 + 1) / 2 = 0.5
    EXPECT_FALSE(std::isinf(aggregated[3]));
    EXPECT_NEAR(aggregated[0], 0.5f, 1e-5f);
}

TEST_F(GradientValidationTest, NegInfGradientHandled) {
    float grads1[NUM_PARAMS], aggregated[NUM_PARAMS];

    for (uint32_t i = 0; i < NUM_PARAMS; i++) {
        grads1[i] = -std::numeric_limits<float>::infinity();
    }

    nimcp_federated_gradient_t devices[1] = {
        make_gradient(1, grads1, NUM_PARAMS),
    };

    int ret = nimcp_federated_aggregate(devices, 1, aggregated, NUM_PARAMS,
                                         NIMCP_FED_AVG);
    EXPECT_EQ(ret, 0);
    // Should not crash
}

TEST_F(GradientValidationTest, ExtremeMagnitudeGradient) {
    float grads1[NUM_PARAMS], grads2[NUM_PARAMS], aggregated[NUM_PARAMS];

    for (uint32_t i = 0; i < NUM_PARAMS; i++) {
        grads1[i] = 1.0f;
        grads2[i] = 1.0f;
    }
    grads1[0] = 1e10f;

    nimcp_federated_gradient_t devices[2] = {
        make_gradient(1, grads1, NUM_PARAMS),
        make_gradient(2, grads2, NUM_PARAMS),
    };

    int ret = nimcp_federated_aggregate(devices, 2, aggregated, NUM_PARAMS,
                                         NIMCP_FED_AVG);
    EXPECT_EQ(ret, 0);

    // Poisoned device (1e10 > 1e6 threshold) zeroed. Result = (0 + 1) / 2 = 0.5
    EXPECT_NEAR(aggregated[0], 0.5f, 1e-5f);
}

TEST_F(GradientValidationTest, NormalMagnitudePreserved) {
    float grads1[NUM_PARAMS], aggregated[NUM_PARAMS];

    for (uint32_t i = 0; i < NUM_PARAMS; i++) {
        grads1[i] = 999999.0f;
    }

    nimcp_federated_gradient_t devices[1] = {
        make_gradient(1, grads1, NUM_PARAMS),
    };

    int ret = nimcp_federated_aggregate(devices, 1, aggregated, NUM_PARAMS,
                                         NIMCP_FED_AVG);
    EXPECT_EQ(ret, 0);

    for (uint32_t i = 0; i < NUM_PARAMS; i++) {
        EXPECT_NEAR(aggregated[i], 999999.0f, 1.0f);
    }
}

TEST_F(GradientValidationTest, MultipleDevicesOnePoisoned) {
    float grads1[NUM_PARAMS], grads2[NUM_PARAMS], grads3[NUM_PARAMS], aggregated[NUM_PARAMS];

    for (uint32_t i = 0; i < NUM_PARAMS; i++) {
        grads1[i] = 2.0f;
        grads2[i] = std::numeric_limits<float>::quiet_NaN();
        grads3[i] = 4.0f;
    }

    nimcp_federated_gradient_t devices[3] = {
        make_gradient(1, grads1, NUM_PARAMS),
        make_gradient(2, grads2, NUM_PARAMS),
        make_gradient(3, grads3, NUM_PARAMS),
    };

    // FedMedian is Byzantine-tolerant - should exclude NaN
    int ret = nimcp_federated_aggregate(devices, 3, aggregated, NUM_PARAMS,
                                         NIMCP_FED_MEDIAN);
    EXPECT_EQ(ret, 0);

    // With median of [2.0, NaN, 4.0], qsort behavior with NaN is implementation-defined
    // but function should not crash
}

TEST_F(GradientValidationTest, AllPoisonedGradients) {
    float grads1[NUM_PARAMS], grads2[NUM_PARAMS], aggregated[NUM_PARAMS];

    for (uint32_t i = 0; i < NUM_PARAMS; i++) {
        grads1[i] = std::numeric_limits<float>::quiet_NaN();
        grads2[i] = std::numeric_limits<float>::quiet_NaN();
    }

    nimcp_federated_gradient_t devices[2] = {
        make_gradient(1, grads1, NUM_PARAMS),
        make_gradient(2, grads2, NUM_PARAMS),
    };

    int ret = nimcp_federated_aggregate(devices, 2, aggregated, NUM_PARAMS,
                                         NIMCP_FED_AVG);
    EXPECT_EQ(ret, 0);
    // All NaN averaged is NaN - should not crash
}

TEST_F(GradientValidationTest, EmptyGradientArray) {
    float aggregated[NUM_PARAMS];
    int ret = nimcp_federated_aggregate(nullptr, 0, aggregated, NUM_PARAMS,
                                         NIMCP_FED_AVG);
    EXPECT_EQ(ret, -1);
}

TEST_F(GradientValidationTest, SingleDeviceNaN) {
    float grads1[NUM_PARAMS], aggregated[NUM_PARAMS];

    for (uint32_t i = 0; i < NUM_PARAMS; i++) {
        grads1[i] = std::numeric_limits<float>::quiet_NaN();
    }

    nimcp_federated_gradient_t devices[1] = {
        make_gradient(1, grads1, NUM_PARAMS),
    };

    int ret = nimcp_federated_aggregate(devices, 1, aggregated, NUM_PARAMS,
                                         NIMCP_FED_AVG);
    EXPECT_EQ(ret, 0);
    // Should not crash, NaN propagates
}

TEST_F(GradientValidationTest, GradientValidationPreservesOrder) {
    const uint32_t N_DEVICES = 5;
    float grads[N_DEVICES][NUM_PARAMS];
    float aggregated[NUM_PARAMS];

    for (uint32_t d = 0; d < N_DEVICES; d++) {
        for (uint32_t i = 0; i < NUM_PARAMS; i++) {
            grads[d][i] = (float)(d * 10 + i);
        }
    }

    nimcp_federated_gradient_t devices[N_DEVICES];
    for (uint32_t d = 0; d < N_DEVICES; d++) {
        devices[d] = make_gradient(d + 1, grads[d], NUM_PARAMS);
    }

    int ret = nimcp_federated_aggregate(devices, N_DEVICES, aggregated, NUM_PARAMS,
                                         NIMCP_FED_AVG);
    EXPECT_EQ(ret, 0);

    // Verify average is correct for each parameter
    for (uint32_t i = 0; i < NUM_PARAMS; i++) {
        float expected = 0;
        for (uint32_t d = 0; d < N_DEVICES; d++) {
            expected += grads[d][i];
        }
        expected /= N_DEVICES;
        EXPECT_NEAR(aggregated[i], expected, 1e-4f);
    }
}

TEST_F(GradientValidationTest, NullGradientPointerRejected) {
    float aggregated[NUM_PARAMS];

    nimcp_federated_gradient_t devices[1];
    memset(&devices[0], 0, sizeof(devices[0]));
    devices[0].device_id = 1;
    devices[0].num_params = NUM_PARAMS;
    devices[0].gradients = nullptr;

    int ret = nimcp_federated_aggregate(devices, 1, aggregated, NUM_PARAMS,
                                         NIMCP_FED_AVG);
    EXPECT_EQ(ret, -1);
}

// --- Federated Blend Tests ---

TEST_F(GradientValidationTest, BlendWithNullWeightsRejected) {
    float master[NUM_PARAMS] = {};
    EXPECT_EQ(nimcp_federated_blend(nullptr, master, NUM_PARAMS, 0.5f, nullptr), -1);
    float device[NUM_PARAMS] = {};
    EXPECT_EQ(nimcp_federated_blend(device, nullptr, NUM_PARAMS, 0.5f, nullptr), -1);
}

TEST_F(GradientValidationTest, BlendWithZeroParamsRejected) {
    float device[1] = {1.0f};
    float master[1] = {2.0f};
    EXPECT_EQ(nimcp_federated_blend(device, master, 0, 0.5f, nullptr), -1);
}

TEST_F(GradientValidationTest, BlendBasicOperation) {
    float device[NUM_PARAMS], master[NUM_PARAMS];

    for (uint32_t i = 0; i < NUM_PARAMS; i++) {
        device[i] = 1.0f;
        master[i] = 3.0f;
    }

    int ret = nimcp_federated_blend(device, master, NUM_PARAMS, 0.5f, nullptr);
    EXPECT_EQ(ret, 0);

    // 0.5 * 1.0 + 0.5 * 3.0 = 2.0
    for (uint32_t i = 0; i < NUM_PARAMS; i++) {
        EXPECT_NEAR(device[i], 2.0f, 1e-5f);
    }
}
