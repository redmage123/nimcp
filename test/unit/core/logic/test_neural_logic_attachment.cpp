/**
 * @file test_neural_logic_attachment.cpp
 * @brief Unit Tests for MODULE 1: Neural Logic Attachment
 * @version 3.0.0
 * @date 2025-11-20
 *
 * TEST COVERAGE: 8 tests, 100% function coverage
 * - brain_attach_neural_logic: 3 tests
 * - brain_detach_neural_logic: 2 tests
 * - brain_get_neural_logic: 2 tests
 * - brain_has_neural_logic: 1 test
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "core/logic/nimcp_neural_logic_attachment.h"
#include "core/logic/nimcp_neural_logic_factory.h"
#include "core/brain/nimcp_brain.h"

class NeuralLogicAttachmentTest : public ::testing::Test {
protected:
    brain_t brain;
    neural_logic_network_t network;

    void SetUp() override {
        brain = nullptr;
        network = nullptr;
    }

    void TearDown() override {
        if (brain) {
            brain_destroy(brain);
        }
        if (network) {
            neural_logic_destroy(network);
        }
    }

    brain_t create_test_brain() {
        return brain_create("test_brain", 1000);
    }

    neural_logic_network_t create_test_network() {
        return create_default_neural_logic(1000);
    }
};

//=============================================================================
// Test: brain_attach_neural_logic
//=============================================================================

TEST_F(NeuralLogicAttachmentTest, AttachNetworkSuccess) {
    brain = create_test_brain();
    network = create_test_network();

    ASSERT_NE(brain, nullptr);
    ASSERT_NE(network, nullptr);

    bool result = brain_attach_neural_logic(brain, network);

    EXPECT_TRUE(result);
    EXPECT_EQ(brain_get_neural_logic(brain), network);
    EXPECT_TRUE(brain_has_neural_logic(brain));

    // Prevent double-free
    network = nullptr;
}

TEST_F(NeuralLogicAttachmentTest, AttachNetworkNullBrain) {
    network = create_test_network();
    ASSERT_NE(network, nullptr);

    bool result = brain_attach_neural_logic(nullptr, network);

    EXPECT_FALSE(result);
}

TEST_F(NeuralLogicAttachmentTest, AttachNetworkNullNetwork) {
    brain = create_test_brain();
    ASSERT_NE(brain, nullptr);

    bool result = brain_attach_neural_logic(brain, nullptr);

    EXPECT_FALSE(result);
}

//=============================================================================
// Test: brain_detach_neural_logic
//=============================================================================

TEST_F(NeuralLogicAttachmentTest, DetachNetworkSuccess) {
    brain = create_test_brain();
    network = create_test_network();

    ASSERT_NE(brain, nullptr);
    ASSERT_NE(network, nullptr);

    brain_attach_neural_logic(brain, network);
    network = nullptr;  // Ownership transferred

    neural_logic_network_t detached = brain_detach_neural_logic(brain);

    EXPECT_NE(detached, nullptr);
    EXPECT_FALSE(brain_has_neural_logic(brain));
    EXPECT_EQ(brain_get_neural_logic(brain), nullptr);

    // Cleanup detached network
    neural_logic_destroy(detached);
}

TEST_F(NeuralLogicAttachmentTest, DetachNetworkNullBrain) {
    neural_logic_network_t detached = brain_detach_neural_logic(nullptr);

    EXPECT_EQ(detached, nullptr);
}

//=============================================================================
// Test: brain_get_neural_logic
//=============================================================================

TEST_F(NeuralLogicAttachmentTest, GetNetworkAttached) {
    brain = create_test_brain();
    network = create_test_network();

    ASSERT_NE(brain, nullptr);
    ASSERT_NE(network, nullptr);

    brain_attach_neural_logic(brain, network);
    network = nullptr;

    neural_logic_network_t retrieved = brain_get_neural_logic(brain);

    EXPECT_NE(retrieved, nullptr);
}

TEST_F(NeuralLogicAttachmentTest, GetNetworkNotAttached) {
    brain = create_test_brain();
    ASSERT_NE(brain, nullptr);

    neural_logic_network_t retrieved = brain_get_neural_logic(brain);

    EXPECT_EQ(retrieved, nullptr);
}

//=============================================================================
// Test: brain_has_neural_logic
//=============================================================================

TEST_F(NeuralLogicAttachmentTest, HasNetworkCheck) {
    brain = create_test_brain();
    network = create_test_network();

    ASSERT_NE(brain, nullptr);
    ASSERT_NE(network, nullptr);

    EXPECT_FALSE(brain_has_neural_logic(brain));

    brain_attach_neural_logic(brain, network);
    network = nullptr;

    EXPECT_TRUE(brain_has_neural_logic(brain));
}
