/**
 * @file test_neural_logic_factory.cpp
 * @brief Unit Tests for MODULE 5: Neural Logic Factory
 * @version 3.0.0
 * @date 2025-11-20
 *
 * TEST COVERAGE: 6 tests, 100% function coverage
 * - create_default_neural_logic: 2 tests
 * - create_neural_logic_with_config: 2 tests
 * - create_and_attach_neural_logic: 1 test
 * - get_default_neural_logic_config: 1 test
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "core/logic/nimcp_neural_logic_factory.h"
#include "core/brain/nimcp_brain.h"

class NeuralLogicFactoryTest : public ::testing::Test {
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
};

//=============================================================================
// Test: create_default_neural_logic
//=============================================================================

TEST_F(NeuralLogicFactoryTest, CreateDefaultSmall) {
    network = create_default_neural_logic(1000);

    EXPECT_NE(network, nullptr);
}

TEST_F(NeuralLogicFactoryTest, CreateDefaultLarge) {
    network = create_default_neural_logic(10000);

    EXPECT_NE(network, nullptr);
}

//=============================================================================
// Test: create_neural_logic_with_config
//=============================================================================

TEST_F(NeuralLogicFactoryTest, CreateWithConfigSuccess) {
    neural_logic_config_t config = get_default_neural_logic_config(500);

    network = create_neural_logic_with_config(&config);

    EXPECT_NE(network, nullptr);
}

TEST_F(NeuralLogicFactoryTest, CreateWithConfigNullConfig) {
    network = create_neural_logic_with_config(nullptr);

    EXPECT_EQ(network, nullptr);
}

//=============================================================================
// Test: create_and_attach_neural_logic
//=============================================================================

TEST_F(NeuralLogicFactoryTest, CreateAndAttachSuccess) {
    brain = brain_create("factory_test_brain", 1000);
    ASSERT_NE(brain, nullptr);

    bool result = create_and_attach_neural_logic(brain, 1000);

    EXPECT_TRUE(result);
    EXPECT_TRUE(brain_has_neural_logic(brain));
}

//=============================================================================
// Test: get_default_neural_logic_config
//=============================================================================

TEST_F(NeuralLogicFactoryTest, GetDefaultConfig) {
    neural_logic_config_t config = get_default_neural_logic_config(1000);

    EXPECT_EQ(config.max_logic_neurons, 1000u);
    EXPECT_EQ(config.max_variables, 26u);
    EXPECT_EQ(config.variable_pattern_dim, 64u);
    EXPECT_EQ(config.threads_per_block, 256u);
    EXPECT_EQ(config.timestep_us, 100.0f);
    EXPECT_EQ(config.integration_window_ms, 10.0f);
    EXPECT_FALSE(config.enable_learning);
}
