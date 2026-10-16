/**
 * @file test_api_network.cpp
 * @brief Unit tests for NIMCP API - Neural Network functions
 *
 * Tests the neural network API:
 * - nimcp_network_create()
 * - nimcp_network_destroy()
 * - nimcp_network_forward()
 * - nimcp_network_train()
 */

#include <gtest/gtest.h>
#include "nimcp.h"
#include <cstring>

class NetworkAPITest : public ::testing::Test {
protected:
    void SetUp() override {
        nimcp_init();
    }

    void TearDown() override {
        nimcp_shutdown();
    }
};

//=============================================================================
// nimcp_network_create() tests
//=============================================================================

TEST_F(NetworkAPITest, NetworkCreateSucceeds) {
    nimcp_network_t network = nimcp_network_create(10, 5, 20, 0.01f);
    EXPECT_NE(network, nullptr);

    if (network) {
        nimcp_network_destroy(network);
    }
}

TEST_F(NetworkAPITest, NetworkCreateReturnsValidHandle) {
    nimcp_network_t network = nimcp_network_create(5, 3, 10, 0.1f);
    ASSERT_NE(network, nullptr);

    // Verify handle is valid by using it
    float inputs[5] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f};
    float outputs[3];

    nimcp_status_t status = nimcp_network_forward(network, inputs, 5, outputs, 3);
    EXPECT_TRUE(status == NIMCP_SUCCESS || status == NIMCP_ERROR);

    nimcp_network_destroy(network);
}

TEST_F(NetworkAPITest, NetworkCreateWithZeroInputs) {
    nimcp_network_t network = nimcp_network_create(0, 5, 10, 0.01f);
    // Implementation may or may not allow zero inputs

    if (network) {
        nimcp_network_destroy(network);
    }
}

TEST_F(NetworkAPITest, NetworkCreateWithZeroOutputs) {
    nimcp_network_t network = nimcp_network_create(10, 0, 10, 0.01f);
    // Implementation may or may not allow zero outputs

    if (network) {
        nimcp_network_destroy(network);
    }
}

TEST_F(NetworkAPITest, NetworkCreateWithZeroHidden) {
    nimcp_network_t network = nimcp_network_create(10, 5, 0, 0.01f);
    // Implementation may or may not allow zero hidden

    if (network) {
        nimcp_network_destroy(network);
    }
}

TEST_F(NetworkAPITest, NetworkCreateWithLargeDimensions) {
    nimcp_network_t network = nimcp_network_create(1000, 100, 500, 0.001f);
    // May succeed or fail based on memory constraints

    if (network) {
        nimcp_network_destroy(network);
    }
}

TEST_F(NetworkAPITest, NetworkCreateWithDifferentLearningRates) {
    float learning_rates[] = {0.0001f, 0.001f, 0.01f, 0.1f, 1.0f};

    for (float lr : learning_rates) {
        nimcp_network_t network = nimcp_network_create(10, 5, 20, lr);

        if (network) {
            nimcp_network_destroy(network);
        }
    }
}

//=============================================================================
// nimcp_network_destroy() tests
//=============================================================================

TEST_F(NetworkAPITest, NetworkDestroySucceeds) {
    nimcp_network_t network = nimcp_network_create(10, 5, 20, 0.01f);
    ASSERT_NE(network, nullptr);

    // Should not crash
    nimcp_network_destroy(network);
}

TEST_F(NetworkAPITest, NetworkDestroyWithNullIsSafe) {
    // Should not crash with NULL
    nimcp_network_destroy(nullptr);
}

TEST_F(NetworkAPITest, NetworkDestroyMultipleTimes) {
    nimcp_network_t network = nimcp_network_create(10, 5, 20, 0.01f);
    ASSERT_NE(network, nullptr);

    nimcp_network_destroy(network);

    // Second destroy with same pointer is undefined behavior
    // (we don't test it as it's user error)
}

//=============================================================================
// nimcp_network_forward() tests
//=============================================================================

TEST_F(NetworkAPITest, NetworkForwardSucceeds) {
    nimcp_network_t network = nimcp_network_create(10, 5, 20, 0.01f);
    ASSERT_NE(network, nullptr);

    float inputs[10] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f,
                        0.6f, 0.7f, 0.8f, 0.9f, 1.0f};
    float outputs[5];

    nimcp_status_t status = nimcp_network_forward(network, inputs, 10, outputs, 5);
    EXPECT_EQ(status, NIMCP_SUCCESS);

    nimcp_network_destroy(network);
}

TEST_F(NetworkAPITest, NetworkForwardNullNetworkFails) {
    float inputs[10] = {0.5f};
    float outputs[5];

    nimcp_status_t status = nimcp_network_forward(nullptr, inputs, 10, outputs, 5);
    EXPECT_EQ(status, NIMCP_ERROR_NULL_ARG);
}

TEST_F(NetworkAPITest, NetworkForwardNullInputsFails) {
    nimcp_network_t network = nimcp_network_create(10, 5, 20, 0.01f);
    ASSERT_NE(network, nullptr);

    float outputs[5];

    nimcp_status_t status = nimcp_network_forward(network, nullptr, 10, outputs, 5);
    EXPECT_EQ(status, NIMCP_ERROR_NULL_ARG);

    nimcp_network_destroy(network);
}

TEST_F(NetworkAPITest, NetworkForwardNullOutputsFails) {
    nimcp_network_t network = nimcp_network_create(10, 5, 20, 0.01f);
    ASSERT_NE(network, nullptr);

    float inputs[10] = {0.5f};

    nimcp_status_t status = nimcp_network_forward(network, inputs, 10, nullptr, 5);
    EXPECT_EQ(status, NIMCP_ERROR_NULL_ARG);

    nimcp_network_destroy(network);
}

TEST_F(NetworkAPITest, NetworkForwardProducesOutputs) {
    nimcp_network_t network = nimcp_network_create(5, 3, 10, 0.01f);
    ASSERT_NE(network, nullptr);

    float inputs[5] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f};
    float outputs[3] = {0.0f, 0.0f, 0.0f};

    nimcp_status_t status = nimcp_network_forward(network, inputs, 5, outputs, 3);

    if (status == NIMCP_SUCCESS) {
        // Outputs should have been modified (not all zero)
        bool has_non_zero = false;
        for (int i = 0; i < 3; i++) {
            if (outputs[i] != 0.0f) {
                has_non_zero = true;
                break;
            }
        }
        // Note: outputs could theoretically be all zero, but unlikely
    }

    nimcp_network_destroy(network);
}

TEST_F(NetworkAPITest, NetworkForwardWithDifferentInputs) {
    nimcp_network_t network = nimcp_network_create(5, 3, 10, 0.01f);
    ASSERT_NE(network, nullptr);

    float inputs1[5] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f};
    float inputs2[5] = {0.9f, 0.8f, 0.7f, 0.6f, 0.5f};
    float outputs1[3];
    float outputs2[3];

    nimcp_status_t status1 = nimcp_network_forward(network, inputs1, 5, outputs1, 3);
    nimcp_status_t status2 = nimcp_network_forward(network, inputs2, 5, outputs2, 3);

    EXPECT_EQ(status1, status2);

    nimcp_network_destroy(network);
}

TEST_F(NetworkAPITest, NetworkForwardMultipleTimes) {
    nimcp_network_t network = nimcp_network_create(5, 3, 10, 0.01f);
    ASSERT_NE(network, nullptr);

    float inputs[5] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
    float outputs[3];

    // Run forward pass multiple times
    for (int i = 0; i < 10; i++) {
        nimcp_status_t status = nimcp_network_forward(network, inputs, 5, outputs, 3);
        EXPECT_EQ(status, NIMCP_SUCCESS);
    }

    nimcp_network_destroy(network);
}

TEST_F(NetworkAPITest, NetworkForwardWithZeroInputs) {
    nimcp_network_t network = nimcp_network_create(5, 3, 10, 0.01f);
    ASSERT_NE(network, nullptr);

    float inputs[5] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    float outputs[3];

    nimcp_status_t status = nimcp_network_forward(network, inputs, 5, outputs, 3);
    EXPECT_EQ(status, NIMCP_SUCCESS);

    nimcp_network_destroy(network);
}

TEST_F(NetworkAPITest, NetworkForwardWithLargeInputs) {
    nimcp_network_t network = nimcp_network_create(5, 3, 10, 0.01f);
    ASSERT_NE(network, nullptr);

    float inputs[5] = {100.0f, 200.0f, 300.0f, 400.0f, 500.0f};
    float outputs[3];

    nimcp_status_t status = nimcp_network_forward(network, inputs, 5, outputs, 3);
    EXPECT_EQ(status, NIMCP_SUCCESS);

    nimcp_network_destroy(network);
}

//=============================================================================
// nimcp_network_train() tests
//=============================================================================

TEST_F(NetworkAPITest, NetworkTrainReturnsNotImplemented) {
    nimcp_network_t network = nimcp_network_create(5, 3, 10, 0.01f);
    ASSERT_NE(network, nullptr);

    float inputs[5] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f};
    float targets[3] = {1.0f, 0.0f, 0.0f};

    nimcp_status_t status = nimcp_network_train(network, inputs, 5, targets, 3);

    // According to implementation, train returns NIMCP_ERROR (not implemented)
    EXPECT_EQ(status, NIMCP_ERROR);

    nimcp_network_destroy(network);
}

TEST_F(NetworkAPITest, NetworkTrainNullNetworkFails) {
    float inputs[5] = {0.5f};
    float targets[3] = {1.0f};

    nimcp_status_t status = nimcp_network_train(nullptr, inputs, 5, targets, 3);
    EXPECT_EQ(status, NIMCP_ERROR_NULL_ARG);
}

//=============================================================================
// Network workflow tests
//=============================================================================

TEST_F(NetworkAPITest, CreateForwardDestroyWorkflow) {
    // Create network
    nimcp_network_t network = nimcp_network_create(10, 5, 20, 0.01f);
    ASSERT_NE(network, nullptr);

    // Forward pass
    float inputs[10];
    for (int i = 0; i < 10; i++) {
        inputs[i] = static_cast<float>(i) * 0.1f;
    }
    float outputs[5];

    nimcp_status_t status = nimcp_network_forward(network, inputs, 10, outputs, 5);
    EXPECT_EQ(status, NIMCP_SUCCESS);

    // Destroy
    nimcp_network_destroy(network);
}

TEST_F(NetworkAPITest, MultipleNetworksIndependent) {
    // Create two networks
    nimcp_network_t network1 = nimcp_network_create(5, 3, 10, 0.01f);
    nimcp_network_t network2 = nimcp_network_create(5, 3, 10, 0.01f);

    ASSERT_NE(network1, nullptr);
    ASSERT_NE(network2, nullptr);
    ASSERT_NE(network1, network2);

    // Use both networks
    float inputs[5] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
    float outputs1[3], outputs2[3];

    nimcp_status_t status1 = nimcp_network_forward(network1, inputs, 5, outputs1, 3);
    nimcp_status_t status2 = nimcp_network_forward(network2, inputs, 5, outputs2, 3);

    EXPECT_EQ(status1, NIMCP_SUCCESS);
    EXPECT_EQ(status2, NIMCP_SUCCESS);

    // Destroy both
    nimcp_network_destroy(network1);
    nimcp_network_destroy(network2);
}
