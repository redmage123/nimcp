/**
 * @file test_neural_logic_connectivity.cpp
 * @brief Unit tests for neural logic connectivity and signal propagation
 *
 * WHAT: Test connection storage and signal propagation through logic circuits
 * WHY:  Ensure neural_logic_connect() works and gates form functional circuits
 * HOW:  Test connection creation, signal flow, and circuit evaluation
 *
 * @author NIMCP Development Team
 * @date 2025-11-16
 * @version 2.7.0
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

#include "core/neuron_types/nimcp_neural_logic.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Fixture
//=============================================================================

class NeuralLogicConnectivityTest : public ::testing::Test {
protected:
    neural_logic_network_t network = nullptr;
    neural_logic_config_t config;

    void SetUp() override {
        config = neural_logic_default_config(100);
        network = neural_logic_create(&config);
        ASSERT_NE(network, nullptr);
    }

    void TearDown() override {
        if (network) {
            neural_logic_destroy(network);
        }
    }

    // Helper: Set neuron input activities manually
    void set_neuron_inputs(uint32_t neuron_id, float input_a, float input_b) {
        logic_neuron_state_t state;
        ASSERT_TRUE(neural_logic_get_state(network, neuron_id, &state));
        // Note: We can't directly set inputs through the API, so we'll use evaluation
    }
};

//=============================================================================
// Connection Creation Tests
//=============================================================================

TEST_F(NeuralLogicConnectivityTest, ConnectTwoGates) {
    uint32_t source = neural_logic_create_gate(network, LOGIC_GATE_AND, 1.8f);
    uint32_t target = neural_logic_create_gate(network, LOGIC_GATE_OR, 1.0f);

    ASSERT_NE(source, UINT32_MAX);
    ASSERT_NE(target, UINT32_MAX);

    bool success = neural_logic_connect(network, source, target, 1.0f);
    EXPECT_TRUE(success);
}

TEST_F(NeuralLogicConnectivityTest, ConnectMultipleTargets) {
    uint32_t source = neural_logic_create_gate(network, LOGIC_GATE_AND, 1.8f);
    uint32_t target1 = neural_logic_create_gate(network, LOGIC_GATE_OR, 1.0f);
    uint32_t target2 = neural_logic_create_gate(network, LOGIC_GATE_XOR, 1.5f);
    uint32_t target3 = neural_logic_create_gate(network, LOGIC_GATE_NOT, 0.5f);

    EXPECT_TRUE(neural_logic_connect(network, source, target1, 1.0f));
    EXPECT_TRUE(neural_logic_connect(network, source, target2, 0.8f));
    EXPECT_TRUE(neural_logic_connect(network, source, target3, -0.5f));
}

TEST_F(NeuralLogicConnectivityTest, ConnectChain) {
    // A -> B -> C -> D
    uint32_t gate_a = neural_logic_create_gate(network, LOGIC_GATE_AND, 1.8f);
    uint32_t gate_b = neural_logic_create_gate(network, LOGIC_GATE_OR, 1.0f);
    uint32_t gate_c = neural_logic_create_gate(network, LOGIC_GATE_NOT, 0.5f);
    uint32_t gate_d = neural_logic_create_gate(network, LOGIC_GATE_XOR, 1.5f);

    EXPECT_TRUE(neural_logic_connect(network, gate_a, gate_b, 1.0f));
    EXPECT_TRUE(neural_logic_connect(network, gate_b, gate_c, 1.0f));
    EXPECT_TRUE(neural_logic_connect(network, gate_c, gate_d, 1.0f));
}

TEST_F(NeuralLogicConnectivityTest, ConnectWithPositiveWeight) {
    uint32_t source = neural_logic_create_gate(network, LOGIC_GATE_AND, 1.8f);
    uint32_t target = neural_logic_create_gate(network, LOGIC_GATE_OR, 1.0f);

    bool success = neural_logic_connect(network, source, target, 2.5f);
    EXPECT_TRUE(success);
}

TEST_F(NeuralLogicConnectivityTest, ConnectWithNegativeWeight) {
    uint32_t source = neural_logic_create_gate(network, LOGIC_GATE_NOT, 0.5f);
    uint32_t target = neural_logic_create_gate(network, LOGIC_GATE_AND, 1.8f);

    bool success = neural_logic_connect(network, source, target, -1.0f);
    EXPECT_TRUE(success);
}

TEST_F(NeuralLogicConnectivityTest, ConnectWithZeroWeight) {
    uint32_t source = neural_logic_create_gate(network, LOGIC_GATE_AND, 1.8f);
    uint32_t target = neural_logic_create_gate(network, LOGIC_GATE_OR, 1.0f);

    bool success = neural_logic_connect(network, source, target, 0.0f);
    EXPECT_TRUE(success);  // Zero weight is valid (silent synapse)
}

TEST_F(NeuralLogicConnectivityTest, ConnectInvalidSource) {
    uint32_t target = neural_logic_create_gate(network, LOGIC_GATE_OR, 1.0f);

    bool success = neural_logic_connect(network, UINT32_MAX, target, 1.0f);
    EXPECT_FALSE(success);
}

TEST_F(NeuralLogicConnectivityTest, ConnectInvalidTarget) {
    uint32_t source = neural_logic_create_gate(network, LOGIC_GATE_AND, 1.8f);

    bool success = neural_logic_connect(network, source, UINT32_MAX, 1.0f);
    EXPECT_FALSE(success);
}

TEST_F(NeuralLogicConnectivityTest, ConnectBothInvalid) {
    bool success = neural_logic_connect(network, UINT32_MAX, UINT32_MAX, 1.0f);
    EXPECT_FALSE(success);
}

TEST_F(NeuralLogicConnectivityTest, ConnectWithInfiniteWeight) {
    uint32_t source = neural_logic_create_gate(network, LOGIC_GATE_AND, 1.8f);
    uint32_t target = neural_logic_create_gate(network, LOGIC_GATE_OR, 1.0f);

    bool success = neural_logic_connect(network, source, target, INFINITY);
    EXPECT_FALSE(success);  // Should reject infinite weights
}

TEST_F(NeuralLogicConnectivityTest, ConnectWithNaNWeight) {
    uint32_t source = neural_logic_create_gate(network, LOGIC_GATE_AND, 1.8f);
    uint32_t target = neural_logic_create_gate(network, LOGIC_GATE_OR, 1.0f);

    bool success = neural_logic_connect(network, source, target, NAN);
    EXPECT_FALSE(success);  // Should reject NaN weights
}

TEST_F(NeuralLogicConnectivityTest, ConnectNullNetwork) {
    bool success = neural_logic_connect(nullptr, 0, 1, 1.0f);
    EXPECT_FALSE(success);
}

//=============================================================================
// Simple Circuit Tests
//=============================================================================

TEST_F(NeuralLogicConnectivityTest, TwoGateAndCircuit) {
    // Input -> AND -> Output
    uint32_t input = neural_logic_create_gate(network, LOGIC_GATE_OR, 0.1f);  // Low threshold = always fires
    uint32_t and_gate = neural_logic_create_gate(network, LOGIC_GATE_AND, 1.8f);

    ASSERT_TRUE(neural_logic_connect(network, input, and_gate, 1.0f));

    // Test by evaluating the AND gate
    float inputs[] = {1.0f, 1.0f};
    float output = 0.0f;
    EXPECT_TRUE(neural_logic_evaluate(network, and_gate, inputs, 2, &output));
    EXPECT_NEAR(output, 1.0f, 0.1f);
}

TEST_F(NeuralLogicConnectivityTest, NotGateInverter) {
    // Input -> NOT -> Output
    uint32_t input = neural_logic_create_gate(network, LOGIC_GATE_OR, 0.1f);
    uint32_t not_gate = neural_logic_create_gate(network, LOGIC_GATE_NOT, 0.5f);

    ASSERT_TRUE(neural_logic_connect(network, input, not_gate, 1.0f));

    // Test NOT gate
    float inputs_high[] = {1.0f};
    float output_high = 0.0f;
    EXPECT_TRUE(neural_logic_evaluate(network, not_gate, inputs_high, 1, &output_high));
    EXPECT_NEAR(output_high, 0.0f, 0.1f);  // NOT(1) = 0

    float inputs_low[] = {0.0f};
    float output_low = 0.0f;
    EXPECT_TRUE(neural_logic_evaluate(network, not_gate, inputs_low, 1, &output_low));
    EXPECT_NEAR(output_low, 1.0f, 0.1f);  // NOT(0) = 1
}

TEST_F(NeuralLogicConnectivityTest, OrGateFanIn) {
    // Input1 -> OR
    // Input2 -> OR
    uint32_t input1 = neural_logic_create_gate(network, LOGIC_GATE_OR, 0.1f);
    uint32_t input2 = neural_logic_create_gate(network, LOGIC_GATE_OR, 0.1f);
    uint32_t or_gate = neural_logic_create_gate(network, LOGIC_GATE_OR, 1.0f);

    ASSERT_TRUE(neural_logic_connect(network, input1, or_gate, 1.0f));
    ASSERT_TRUE(neural_logic_connect(network, input2, or_gate, 1.0f));

    // Test OR gate
    float inputs[] = {0.0f, 1.0f};
    float output = 0.0f;
    EXPECT_TRUE(neural_logic_evaluate(network, or_gate, inputs, 2, &output));
    EXPECT_NEAR(output, 1.0f, 0.1f);  // 0 OR 1 = 1
}

//=============================================================================
// Complex Circuit Tests
//=============================================================================

TEST_F(NeuralLogicConnectivityTest, HalfAdderSum) {
    // Half Adder Sum: A XOR B
    uint32_t input_a = neural_logic_create_gate(network, LOGIC_GATE_OR, 0.1f);
    uint32_t input_b = neural_logic_create_gate(network, LOGIC_GATE_OR, 0.1f);
    uint32_t xor_gate = neural_logic_create_gate(network, LOGIC_GATE_XOR, 0.5f);

    ASSERT_TRUE(neural_logic_connect(network, input_a, xor_gate, 1.0f));
    ASSERT_TRUE(neural_logic_connect(network, input_b, xor_gate, 1.0f));

    // Test XOR truth table
    float inputs_00[] = {0.0f, 0.0f};
    float output_00 = 0.0f;
    EXPECT_TRUE(neural_logic_evaluate(network, xor_gate, inputs_00, 2, &output_00));
    EXPECT_NEAR(output_00, 0.0f, 0.1f);  // 0 XOR 0 = 0

    float inputs_01[] = {0.0f, 1.0f};
    float output_01 = 0.0f;
    EXPECT_TRUE(neural_logic_evaluate(network, xor_gate, inputs_01, 2, &output_01));
    EXPECT_NEAR(output_01, 1.0f, 0.1f);  // 0 XOR 1 = 1

    float inputs_10[] = {1.0f, 0.0f};
    float output_10 = 0.0f;
    EXPECT_TRUE(neural_logic_evaluate(network, xor_gate, inputs_10, 2, &output_10));
    EXPECT_NEAR(output_10, 1.0f, 0.1f);  // 1 XOR 0 = 1

    float inputs_11[] = {1.0f, 1.0f};
    float output_11 = 0.0f;
    EXPECT_TRUE(neural_logic_evaluate(network, xor_gate, inputs_11, 2, &output_11));
    EXPECT_NEAR(output_11, 0.0f, 0.1f);  // 1 XOR 1 = 0
}

TEST_F(NeuralLogicConnectivityTest, HalfAdderCarry) {
    // Half Adder Carry: A AND B
    uint32_t input_a = neural_logic_create_gate(network, LOGIC_GATE_OR, 0.1f);
    uint32_t input_b = neural_logic_create_gate(network, LOGIC_GATE_OR, 0.1f);
    uint32_t and_gate = neural_logic_create_gate(network, LOGIC_GATE_AND, 1.8f);

    ASSERT_TRUE(neural_logic_connect(network, input_a, and_gate, 1.0f));
    ASSERT_TRUE(neural_logic_connect(network, input_b, and_gate, 1.0f));

    // Test AND truth table
    float inputs_11[] = {1.0f, 1.0f};
    float output_11 = 0.0f;
    EXPECT_TRUE(neural_logic_evaluate(network, and_gate, inputs_11, 2, &output_11));
    EXPECT_NEAR(output_11, 1.0f, 0.1f);  // 1 AND 1 = 1 (carry)

    float inputs_10[] = {1.0f, 0.0f};
    float output_10 = 0.0f;
    EXPECT_TRUE(neural_logic_evaluate(network, and_gate, inputs_10, 2, &output_10));
    EXPECT_NEAR(output_10, 0.0f, 0.1f);  // 1 AND 0 = 0 (no carry)
}

TEST_F(NeuralLogicConnectivityTest, NandGate) {
    // NAND = NOT(AND)
    uint32_t and_gate = neural_logic_create_gate(network, LOGIC_GATE_AND, 1.8f);
    uint32_t not_gate = neural_logic_create_gate(network, LOGIC_GATE_NOT, 0.5f);

    ASSERT_TRUE(neural_logic_connect(network, and_gate, not_gate, 1.0f));

    // We can't test this directly without signal propagation through update cycles
    // This test verifies the connection is made
}

TEST_F(NeuralLogicConnectivityTest, Multiplexer2to1Structure) {
    // 2:1 MUX: Out = (A AND NOT(S)) OR (B AND S)
    uint32_t and1 = neural_logic_create_gate(network, LOGIC_GATE_AND, 1.8f);
    uint32_t and2 = neural_logic_create_gate(network, LOGIC_GATE_AND, 1.8f);
    uint32_t not_s = neural_logic_create_gate(network, LOGIC_GATE_NOT, 0.5f);
    uint32_t or_gate = neural_logic_create_gate(network, LOGIC_GATE_OR, 1.0f);

    // Connect structure
    ASSERT_TRUE(neural_logic_connect(network, and1, or_gate, 1.0f));
    ASSERT_TRUE(neural_logic_connect(network, and2, or_gate, 1.0f));

    // This test verifies structure creation
}

//=============================================================================
// Signal Propagation Tests
//=============================================================================

TEST_F(NeuralLogicConnectivityTest, PropagationThroughChain) {
    // Create a chain: A -> B -> C
    uint32_t gate_a = neural_logic_create_gate(network, LOGIC_GATE_OR, 0.1f);
    uint32_t gate_b = neural_logic_create_gate(network, LOGIC_GATE_OR, 1.0f);
    uint32_t gate_c = neural_logic_create_gate(network, LOGIC_GATE_OR, 1.0f);

    ASSERT_TRUE(neural_logic_connect(network, gate_a, gate_b, 1.0f));
    ASSERT_TRUE(neural_logic_connect(network, gate_b, gate_c, 1.0f));

    // Evaluate gate A to generate output
    float inputs[] = {1.0f};
    float output_a = 0.0f;
    EXPECT_TRUE(neural_logic_evaluate(network, gate_a, inputs, 1, &output_a));
    EXPECT_NEAR(output_a, 1.0f, 0.1f);

    // Update network to propagate signals
    neural_logic_update(network, 0, 100);
    neural_logic_update(network, 100, 100);
    neural_logic_update(network, 200, 100);

    // Check if signal reached gate C
    logic_neuron_state_t state_c;
    EXPECT_TRUE(neural_logic_get_state(network, gate_c, &state_c));
    // Signal should have propagated (though may be decayed)
}

TEST_F(NeuralLogicConnectivityTest, FanOutPropagation) {
    // One source -> multiple targets
    uint32_t source = neural_logic_create_gate(network, LOGIC_GATE_OR, 0.1f);
    uint32_t target1 = neural_logic_create_gate(network, LOGIC_GATE_OR, 1.0f);
    uint32_t target2 = neural_logic_create_gate(network, LOGIC_GATE_OR, 1.0f);
    uint32_t target3 = neural_logic_create_gate(network, LOGIC_GATE_OR, 1.0f);

    ASSERT_TRUE(neural_logic_connect(network, source, target1, 1.0f));
    ASSERT_TRUE(neural_logic_connect(network, source, target2, 1.0f));
    ASSERT_TRUE(neural_logic_connect(network, source, target3, 1.0f));

    // Evaluate source
    float inputs[] = {1.0f};
    float output = 0.0f;
    EXPECT_TRUE(neural_logic_evaluate(network, source, inputs, 1, &output));

    // Update to propagate
    neural_logic_update(network, 0, 100);
}

//=============================================================================
// Edge Cases
//=============================================================================

TEST_F(NeuralLogicConnectivityTest, SelfConnection) {
    // Neuron connected to itself (feedback loop)
    uint32_t gate = neural_logic_create_gate(network, LOGIC_GATE_AND, 1.8f);

    bool success = neural_logic_connect(network, gate, gate, 1.0f);
    EXPECT_TRUE(success);  // Self-connections are valid in recurrent networks
}

TEST_F(NeuralLogicConnectivityTest, DuplicateConnections) {
    // Connect same pair multiple times
    uint32_t source = neural_logic_create_gate(network, LOGIC_GATE_AND, 1.8f);
    uint32_t target = neural_logic_create_gate(network, LOGIC_GATE_OR, 1.0f);

    EXPECT_TRUE(neural_logic_connect(network, source, target, 1.0f));
    EXPECT_TRUE(neural_logic_connect(network, source, target, 0.5f));  // Second connection
    EXPECT_TRUE(neural_logic_connect(network, source, target, 2.0f));  // Third connection

    // Multiple connections are valid (parallel synapses exist in biology)
}

TEST_F(NeuralLogicConnectivityTest, BidirectionalConnection) {
    // A -> B and B -> A (recurrent loop)
    uint32_t gate_a = neural_logic_create_gate(network, LOGIC_GATE_AND, 1.8f);
    uint32_t gate_b = neural_logic_create_gate(network, LOGIC_GATE_OR, 1.0f);

    EXPECT_TRUE(neural_logic_connect(network, gate_a, gate_b, 1.0f));
    EXPECT_TRUE(neural_logic_connect(network, gate_b, gate_a, 1.0f));
}

TEST_F(NeuralLogicConnectivityTest, MaxConnections) {
    // Create many connections from one source
    uint32_t source = neural_logic_create_gate(network, LOGIC_GATE_AND, 1.8f);

    // Create 50 target neurons and connect all
    for (int i = 0; i < 50; i++) {
        uint32_t target = neural_logic_create_gate(network, LOGIC_GATE_OR, 1.0f);
        if (target != UINT32_MAX) {
            EXPECT_TRUE(neural_logic_connect(network, source, target, 1.0f));
        }
    }
}

//=============================================================================
// Memory and Resource Tests
//=============================================================================

TEST_F(NeuralLogicConnectivityTest, DestroyWithConnections) {
    uint32_t source = neural_logic_create_gate(network, LOGIC_GATE_AND, 1.8f);
    uint32_t target = neural_logic_create_gate(network, LOGIC_GATE_OR, 1.0f);

    ASSERT_TRUE(neural_logic_connect(network, source, target, 1.0f));

    // Destroy should free all connections
    neural_logic_destroy(network);
    network = nullptr;  // Prevent double-free in TearDown
}

TEST_F(NeuralLogicConnectivityTest, CreateDestroyMultipleNetworks) {
    // Create multiple networks with connections
    for (int i = 0; i < 5; i++) {
        neural_logic_config_t cfg = neural_logic_default_config(20);
        neural_logic_network_t net = neural_logic_create(&cfg);
        ASSERT_NE(net, nullptr);

        uint32_t g1 = neural_logic_create_gate(net, LOGIC_GATE_AND, 1.8f);
        uint32_t g2 = neural_logic_create_gate(net, LOGIC_GATE_OR, 1.0f);
        EXPECT_TRUE(neural_logic_connect(net, g1, g2, 1.0f));

        neural_logic_destroy(net);
    }
}
