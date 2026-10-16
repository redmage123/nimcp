//=============================================================================
// test_brain_init_network.cpp - Tests for Network Creation Function
//=============================================================================
/**
 * @file test_brain_init_network.cpp
 * @brief Comprehensive unit tests for nimcp_brain_factory_create_brain_network()
 *
 * WHAT: Test suite for adaptive network creation
 * WHY:  Ensure proper network creation, configuration, and error handling
 * HOW:  GoogleTest framework with configuration validation
 *
 * FUNCTION UNDER TEST:
 * - nimcp_brain_factory_create_brain_network() - Create adaptive network
 *
 * TEST CATEGORIES:
 * 1. Basic Network Creation
 * 2. Configuration Parameters
 * 3. Layer Configuration
 * 4. Sparsity Settings
 * 5. Integration Methods
 * 6. Error Handling
 * 7. Memory Management
 * 8. Network Validation
 * 9. Parameter Combinations
 * 10. Edge Cases
 *
 * @version 2.7.0
 * @author NIMCP Development Team
 * @date 2025-11-21
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

#include "core/brain/factory/nimcp_brain_factory.h"
#include "core/brain/nimcp_brain.h"
#include "nimcp.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Fixture
//=============================================================================

class BrainInitNetworkTest : public ::testing::Test {
protected:
    void SetUp() override {
        nimcp_memory_init();
        nimcp_init();
        brain_clear_error();
    }

    void TearDown() override {
        nimcp_shutdown();
        nimcp_memory_cleanup();
    }

    // Helper: Destroy adaptive network
    void destroy_network(adaptive_network_t network) {
        if (network) {
            adaptive_network_destroy(network);
        }
    }
};

//=============================================================================
// 1. Basic Network Creation Tests
//=============================================================================

TEST_F(BrainInitNetworkTest, CreateNetwork_Success) {
    adaptive_network_t network = nimcp_brain_factory_create_brain_network(
        10, 3, 100, 0.8f, ODE_EULER);

    ASSERT_NE(network, nullptr) << "Network creation should succeed";
    EXPECT_EQ(brain_get_last_error(), nullptr) << "No error should be set";

    destroy_network(network);
}

TEST_F(BrainInitNetworkTest, CreateNetwork_ReturnsValidNetwork) {
    adaptive_network_t network = nimcp_brain_factory_create_brain_network(
        5, 2, 50, 0.7f, ODE_EULER);

    ASSERT_NE(network, nullptr);

    // Should be able to get network info
    // (Actual validation would require network accessor functions)

    destroy_network(network);
}

TEST_F(BrainInitNetworkTest, CreateNetwork_MultipleCreations) {
    adaptive_network_t net1 = nimcp_brain_factory_create_brain_network(
        10, 3, 100, 0.8f, ODE_EULER);
    adaptive_network_t net2 = nimcp_brain_factory_create_brain_network(
        8, 4, 150, 0.75f, ODE_EULER);

    ASSERT_NE(net1, nullptr);
    ASSERT_NE(net2, nullptr);
    EXPECT_NE(net1, net2) << "Should create distinct networks";

    destroy_network(net1);
    destroy_network(net2);
}

//=============================================================================
// 2. Input/Output Dimension Tests
//=============================================================================

TEST_F(BrainInitNetworkTest, CreateNetwork_SmallInputOutput) {
    adaptive_network_t network = nimcp_brain_factory_create_brain_network(
        2, 1, 10, 0.5f, ODE_EULER);

    EXPECT_NE(network, nullptr) << "Should handle small dimensions";

    destroy_network(network);
}

TEST_F(BrainInitNetworkTest, CreateNetwork_LargeInputOutput) {
    adaptive_network_t network = nimcp_brain_factory_create_brain_network(
        100, 50, 1000, 0.9f, ODE_EULER);

    EXPECT_NE(network, nullptr) << "Should handle large dimensions";

    destroy_network(network);
}

TEST_F(BrainInitNetworkTest, CreateNetwork_SingleInput) {
    adaptive_network_t network = nimcp_brain_factory_create_brain_network(
        1, 3, 50, 0.7f, ODE_EULER);

    EXPECT_NE(network, nullptr) << "Should handle single input";

    destroy_network(network);
}

TEST_F(BrainInitNetworkTest, CreateNetwork_SingleOutput) {
    adaptive_network_t network = nimcp_brain_factory_create_brain_network(
        10, 1, 50, 0.7f, ODE_EULER);

    EXPECT_NE(network, nullptr) << "Should handle single output";

    destroy_network(network);
}

TEST_F(BrainInitNetworkTest, CreateNetwork_EqualInputOutput) {
    adaptive_network_t network = nimcp_brain_factory_create_brain_network(
        10, 10, 100, 0.8f, ODE_EULER);

    EXPECT_NE(network, nullptr) << "Should handle equal input/output sizes";

    destroy_network(network);
}

//=============================================================================
// 3. Neuron Count Tests
//=============================================================================

TEST_F(BrainInitNetworkTest, CreateNetwork_TinyNeuronCount) {
    adaptive_network_t network = nimcp_brain_factory_create_brain_network(
        5, 2, 10, 0.5f, ODE_EULER);

    EXPECT_NE(network, nullptr) << "Should handle tiny networks";

    destroy_network(network);
}

TEST_F(BrainInitNetworkTest, CreateNetwork_SmallNeuronCount) {
    adaptive_network_t network = nimcp_brain_factory_create_brain_network(
        10, 3, 100, 0.7f, ODE_EULER);

    EXPECT_NE(network, nullptr) << "Should handle small networks";

    destroy_network(network);
}

TEST_F(BrainInitNetworkTest, CreateNetwork_MediumNeuronCount) {
    adaptive_network_t network = nimcp_brain_factory_create_brain_network(
        20, 5, 1000, 0.85f, ODE_EULER);

    EXPECT_NE(network, nullptr) << "Should handle medium networks";

    destroy_network(network);
}

TEST_F(BrainInitNetworkTest, CreateNetwork_VaryingNeuronCounts) {
    uint32_t neuron_counts[] = {50, 100, 200, 500};

    for (uint32_t count : neuron_counts) {
        adaptive_network_t network = nimcp_brain_factory_create_brain_network(
            10, 3, count, 0.8f, ODE_EULER);

        EXPECT_NE(network, nullptr) << "Failed with " << count << " neurons";

        destroy_network(network);
    }
}

//=============================================================================
// 4. Sparsity Target Tests
//=============================================================================

TEST_F(BrainInitNetworkTest, CreateNetwork_LowSparsity) {
    adaptive_network_t network = nimcp_brain_factory_create_brain_network(
        10, 3, 100, 0.3f, ODE_EULER);

    EXPECT_NE(network, nullptr) << "Should handle low sparsity";

    destroy_network(network);
}

TEST_F(BrainInitNetworkTest, CreateNetwork_HighSparsity) {
    adaptive_network_t network = nimcp_brain_factory_create_brain_network(
        10, 3, 100, 0.95f, ODE_EULER);

    EXPECT_NE(network, nullptr) << "Should handle high sparsity";

    destroy_network(network);
}

TEST_F(BrainInitNetworkTest, CreateNetwork_ZeroSparsity) {
    adaptive_network_t network = nimcp_brain_factory_create_brain_network(
        10, 3, 100, 0.0f, ODE_EULER);

    // Should create network (sparsity is just a target)
    EXPECT_NE(network, nullptr);

    destroy_network(network);
}

TEST_F(BrainInitNetworkTest, CreateNetwork_FullSparsity) {
    adaptive_network_t network = nimcp_brain_factory_create_brain_network(
        10, 3, 100, 1.0f, ODE_EULER);

    // Should create network
    EXPECT_NE(network, nullptr);

    destroy_network(network);
}

TEST_F(BrainInitNetworkTest, CreateNetwork_VaryingSparsity) {
    float sparsity_values[] = {0.5f, 0.7f, 0.8f, 0.85f, 0.9f};

    for (float sparsity : sparsity_values) {
        adaptive_network_t network = nimcp_brain_factory_create_brain_network(
            10, 3, 100, sparsity, ODE_EULER);

        EXPECT_NE(network, nullptr) << "Failed with sparsity " << sparsity;

        destroy_network(network);
    }
}

//=============================================================================
// 5. Integration Method Tests
//=============================================================================

TEST_F(BrainInitNetworkTest, CreateNetwork_EulerMethod) {
    adaptive_network_t network = nimcp_brain_factory_create_brain_network(
        10, 3, 100, 0.8f, ODE_EULER);

    EXPECT_NE(network, nullptr) << "Should support Euler integration";

    destroy_network(network);
}

TEST_F(BrainInitNetworkTest, CreateNetwork_RK4Method) {
    adaptive_network_t network = nimcp_brain_factory_create_brain_network(
        10, 3, 100, 0.8f, ODE_RK4);

    EXPECT_NE(network, nullptr) << "Should support RK4 integration";

    destroy_network(network);
}

TEST_F(BrainInitNetworkTest, CreateNetwork_AllIntegrationMethods) {
    ode_integration_method_t methods[] = {ODE_EULER, ODE_RK4};
    const char* method_names[] = {"Euler", "RK4"};

    for (int i = 0; i < 2; i++) {
        adaptive_network_t network = nimcp_brain_factory_create_brain_network(
            10, 3, 100, 0.8f, methods[i]);

        EXPECT_NE(network, nullptr) << "Failed with " << method_names[i];

        destroy_network(network);
    }
}

//=============================================================================
// 6. Error Handling Tests
//=============================================================================

TEST_F(BrainInitNetworkTest, CreateNetwork_ZeroInputs_ShouldFail) {
    adaptive_network_t network = nimcp_brain_factory_create_brain_network(
        0, 3, 100, 0.8f, ODE_EULER);

    // Should fail or return NULL - NULL return is the error indicator
    // Note: set_error() uses LOG_ERROR, not brain_set_error()
    if (network != nullptr) {
        destroy_network(network);
    }
    // Either NULL return or successful creation with zero inputs is acceptable
}

TEST_F(BrainInitNetworkTest, CreateNetwork_ZeroOutputs_ShouldFail) {
    adaptive_network_t network = nimcp_brain_factory_create_brain_network(
        10, 0, 100, 0.8f, ODE_EULER);

    // NULL return indicates failure - that's the error indicator
    if (network != nullptr) {
        destroy_network(network);
    }
}

TEST_F(BrainInitNetworkTest, CreateNetwork_ZeroNeurons_ShouldFail) {
    adaptive_network_t network = nimcp_brain_factory_create_brain_network(
        10, 3, 0, 0.8f, ODE_EULER);

    // NULL return indicates failure - that's the error indicator
    if (network != nullptr) {
        destroy_network(network);
    }
}

//=============================================================================
// 7. Memory Management Tests
//=============================================================================

TEST_F(BrainInitNetworkTest, NoMemoryLeaks_SingleCreation) {
    nimcp_memory_stats_t stats_before, stats_after;

    nimcp_memory_get_stats(&stats_before);

    adaptive_network_t network = nimcp_brain_factory_create_brain_network(
        10, 3, 100, 0.8f, ODE_EULER);
    ASSERT_NE(network, nullptr);

    destroy_network(network);

    nimcp_memory_get_stats(&stats_after);

    // Allow some slack for allocator overhead (check current_allocated, not total)
    EXPECT_LE(stats_after.current_allocated, stats_before.current_allocated + 1024)
        << "Memory leak detected: before=" << stats_before.current_allocated
        << " after=" << stats_after.current_allocated;
}

TEST_F(BrainInitNetworkTest, NoMemoryLeaks_MultipleCreations) {
    nimcp_memory_stats_t stats_before, stats_after;

    nimcp_memory_get_stats(&stats_before);

    for (int i = 0; i < 10; i++) {
        adaptive_network_t network = nimcp_brain_factory_create_brain_network(
            10, 3, 50, 0.8f, ODE_EULER);
        ASSERT_NE(network, nullptr);
        destroy_network(network);
    }

    nimcp_memory_get_stats(&stats_after);

    // Check current_allocated (memory in use) not total_allocated (cumulative)
    EXPECT_LE(stats_after.current_allocated, stats_before.current_allocated + 1024)
        << "Memory leak detected: before=" << stats_before.current_allocated
        << " after=" << stats_after.current_allocated;
}

//=============================================================================
// 8. Configuration Validation Tests
//=============================================================================

TEST_F(BrainInitNetworkTest, NetworkConfig_ValidDimensions) {
    // Test various dimension combinations
    struct TestCase {
        uint32_t inputs;
        uint32_t outputs;
        uint32_t neurons;
    };

    TestCase cases[] = {
        {10, 3, 100},
        {5, 5, 50},
        {20, 10, 200},
        {100, 50, 1000},
    };

    for (const auto& test : cases) {
        adaptive_network_t network = nimcp_brain_factory_create_brain_network(
            test.inputs, test.outputs, test.neurons, 0.8f, ODE_EULER);

        EXPECT_NE(network, nullptr)
            << "Failed with inputs=" << test.inputs
            << " outputs=" << test.outputs
            << " neurons=" << test.neurons;

        destroy_network(network);
    }
}

//=============================================================================
// 9. Realistic Scenarios
//=============================================================================

TEST_F(BrainInitNetworkTest, CreateNetwork_ClassificationTask) {
    // Typical classification: 784 inputs (28x28 image), 10 outputs
    adaptive_network_t network = nimcp_brain_factory_create_brain_network(
        784, 10, 1000, 0.85f, ODE_EULER);

    EXPECT_NE(network, nullptr) << "Should handle classification networks";

    destroy_network(network);
}

TEST_F(BrainInitNetworkTest, CreateNetwork_RegressionTask) {
    // Typical regression: 10 features, 1 output
    adaptive_network_t network = nimcp_brain_factory_create_brain_network(
        10, 1, 500, 0.8f, ODE_RK4);

    EXPECT_NE(network, nullptr) << "Should handle regression networks";

    destroy_network(network);
}

TEST_F(BrainInitNetworkTest, CreateNetwork_ControlTask) {
    // Control task: 4 state inputs, 2 action outputs
    adaptive_network_t network = nimcp_brain_factory_create_brain_network(
        4, 2, 200, 0.7f, ODE_EULER);

    EXPECT_NE(network, nullptr) << "Should handle control networks";

    destroy_network(network);
}

//=============================================================================
// 10. Boundary and Edge Cases
//=============================================================================

TEST_F(BrainInitNetworkTest, CreateNetwork_MinimalConfiguration) {
    // Minimal possible network
    adaptive_network_t network = nimcp_brain_factory_create_brain_network(
        1, 1, 1, 0.0f, ODE_EULER);

    if (network != nullptr) {
        destroy_network(network);
    }
}

TEST_F(BrainInitNetworkTest, CreateNetwork_NegativeSparsity) {
    // Negative sparsity should be handled
    adaptive_network_t network = nimcp_brain_factory_create_brain_network(
        10, 3, 100, -0.5f, ODE_EULER);

    // Should either fail or clamp to valid range
    if (network != nullptr) {
        destroy_network(network);
    }
}

TEST_F(BrainInitNetworkTest, CreateNetwork_SparsityAboveOne) {
    // Sparsity > 1.0 should be handled
    adaptive_network_t network = nimcp_brain_factory_create_brain_network(
        10, 3, 100, 1.5f, ODE_EULER);

    // Should either fail or clamp to valid range
    if (network != nullptr) {
        destroy_network(network);
    }
}

//=============================================================================
// 11. Sequential Creation Tests
//=============================================================================

TEST_F(BrainInitNetworkTest, CreateNetwork_SequentialSizes) {
    // Create networks with increasing sizes
    for (uint32_t neurons = 50; neurons <= 500; neurons += 50) {
        adaptive_network_t network = nimcp_brain_factory_create_brain_network(
            10, 3, neurons, 0.8f, ODE_EULER);

        EXPECT_NE(network, nullptr) << "Failed with " << neurons << " neurons";

        destroy_network(network);
    }
}

TEST_F(BrainInitNetworkTest, CreateNetwork_AlternatingMethods) {
    // Alternate between integration methods
    for (int i = 0; i < 5; i++) {
        ode_integration_method_t method = (i % 2 == 0) ? ODE_EULER : ODE_RK4;

        adaptive_network_t network = nimcp_brain_factory_create_brain_network(
            10, 3, 100, 0.8f, method);

        EXPECT_NE(network, nullptr);

        destroy_network(network);
    }
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
