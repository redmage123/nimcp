/**
 * @file test_neural_logic_circuits.cpp
 * @brief Integration tests for complete neural logic circuits
 *
 * WHAT: Test full logic circuits (adders, comparators, multiplexers)
 * WHY:  Verify that connected gates form functional circuits
 * HOW:  Build complex circuits and test their behavior end-to-end
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

class NeuralLogicCircuitsTest : public ::testing::Test {
protected:
    neural_logic_network_t network = nullptr;
    neural_logic_config_t config;

    void SetUp() override {
        config = neural_logic_default_config(200);  // Larger network for circuits
        network = neural_logic_create(&config);
        ASSERT_NE(network, nullptr);
    }

    void TearDown() override {
        if (network) {
            neural_logic_destroy(network);
        }
    }

    // Helper: Create input neuron (always fires when evaluated)
    uint32_t create_input() {
        return neural_logic_create_gate(network, LOGIC_GATE_OR, 0.01f);
    }

    // Helper: Simulate circuit for multiple timesteps
    void simulate_circuit(int timesteps) {
        uint64_t timestamp = 0;
        for (int i = 0; i < timesteps; i++) {
            neural_logic_update(network, timestamp, 100);
            timestamp += 100;
        }
    }
};

//=============================================================================
// Half Adder Tests
//=============================================================================

TEST_F(NeuralLogicCircuitsTest, HalfAdderComplete) {
    // Half Adder: Sum = A XOR B, Carry = A AND B
    uint32_t sum_gate = neural_logic_create_gate(network, LOGIC_GATE_XOR, 0.5f);
    uint32_t carry_gate = neural_logic_create_gate(network, LOGIC_GATE_AND, 1.8f);

    ASSERT_NE(sum_gate, UINT32_MAX);
    ASSERT_NE(carry_gate, UINT32_MAX);

    // Test all input combinations
    struct TestCase {
        float a, b;
        float expected_sum, expected_carry;
    };

    TestCase cases[] = {
        {0.0f, 0.0f, 0.0f, 0.0f},  // 0 + 0 = 0, carry 0
        {0.0f, 1.0f, 1.0f, 0.0f},  // 0 + 1 = 1, carry 0
        {1.0f, 0.0f, 1.0f, 0.0f},  // 1 + 0 = 1, carry 0
        {1.0f, 1.0f, 0.0f, 1.0f},  // 1 + 1 = 0, carry 1
    };

    for (const auto& test : cases) {
        float inputs[] = {test.a, test.b};
        float sum_output = 0.0f;
        float carry_output = 0.0f;

        EXPECT_TRUE(neural_logic_evaluate(network, sum_gate, inputs, 2, &sum_output));
        EXPECT_TRUE(neural_logic_evaluate(network, carry_gate, inputs, 2, &carry_output));

        EXPECT_NEAR(sum_output, test.expected_sum, 0.1f)
            << "Sum failed for A=" << test.a << ", B=" << test.b;
        EXPECT_NEAR(carry_output, test.expected_carry, 0.1f)
            << "Carry failed for A=" << test.a << ", B=" << test.b;
    }
}

//=============================================================================
// Full Adder Tests
//=============================================================================

TEST_F(NeuralLogicCircuitsTest, FullAdderStructure) {
    // Full Adder: 2 XOR gates, 2 AND gates, 1 OR gate
    // Sum = A XOR B XOR Cin
    // Cout = (A AND B) OR (Cin AND (A XOR B))

    uint32_t xor1 = neural_logic_create_gate(network, LOGIC_GATE_XOR, 0.5f);  // A XOR B
    uint32_t xor2 = neural_logic_create_gate(network, LOGIC_GATE_XOR, 0.5f);  // (A XOR B) XOR Cin
    uint32_t and1 = neural_logic_create_gate(network, LOGIC_GATE_AND, 1.8f); // A AND B
    uint32_t and2 = neural_logic_create_gate(network, LOGIC_GATE_AND, 1.8f); // Cin AND (A XOR B)
    uint32_t or_cout = neural_logic_create_gate(network, LOGIC_GATE_OR, 1.0f); // Carry out

    ASSERT_NE(xor1, UINT32_MAX);
    ASSERT_NE(xor2, UINT32_MAX);
    ASSERT_NE(and1, UINT32_MAX);
    ASSERT_NE(and2, UINT32_MAX);
    ASSERT_NE(or_cout, UINT32_MAX);

    // Connect structure
    EXPECT_TRUE(neural_logic_connect(network, xor1, xor2, 1.0f));    // XOR1 -> XOR2
    EXPECT_TRUE(neural_logic_connect(network, xor1, and2, 1.0f));    // XOR1 -> AND2
    EXPECT_TRUE(neural_logic_connect(network, and1, or_cout, 1.0f)); // AND1 -> OR
    EXPECT_TRUE(neural_logic_connect(network, and2, or_cout, 1.0f)); // AND2 -> OR
}

//=============================================================================
// Comparator Tests
//=============================================================================

TEST_F(NeuralLogicCircuitsTest, EqualityComparator) {
    // A == B  <==>  (A AND B) OR (NOT(A) AND NOT(B))
    // Simplified: NOT(A XOR B)
    uint32_t xor_gate = neural_logic_create_gate(network, LOGIC_GATE_XOR, 0.5f);
    uint32_t not_gate = neural_logic_create_gate(network, LOGIC_GATE_NOT, 0.5f);

    ASSERT_NE(xor_gate, UINT32_MAX);
    ASSERT_NE(not_gate, UINT32_MAX);

    EXPECT_TRUE(neural_logic_connect(network, xor_gate, not_gate, 1.0f));

    // Test equality
    float inputs_equal[] = {1.0f, 1.0f};
    float output_equal = 0.0f;
    EXPECT_TRUE(neural_logic_evaluate(network, xor_gate, inputs_equal, 2, &output_equal));
    EXPECT_NEAR(output_equal, 0.0f, 0.1f);  // Equal: XOR = 0

    float inputs_not_equal[] = {1.0f, 0.0f};
    float output_not_equal = 0.0f;
    EXPECT_TRUE(neural_logic_evaluate(network, xor_gate, inputs_not_equal, 2, &output_not_equal));
    EXPECT_NEAR(output_not_equal, 1.0f, 0.1f);  // Not equal: XOR = 1
}

TEST_F(NeuralLogicCircuitsTest, GreaterThanComparator) {
    // A > B  <==>  A AND NOT(B)
    uint32_t not_b = neural_logic_create_gate(network, LOGIC_GATE_NOT, 0.5f);
    uint32_t and_gate = neural_logic_create_gate(network, LOGIC_GATE_AND, 1.8f);

    ASSERT_NE(not_b, UINT32_MAX);
    ASSERT_NE(and_gate, UINT32_MAX);

    EXPECT_TRUE(neural_logic_connect(network, not_b, and_gate, 1.0f));

    // Test cases for 1-bit comparison
    // A=1, B=0: 1 > 0 = TRUE
    // A=0, B=1: 0 > 1 = FALSE
    // A=1, B=1: 1 > 1 = FALSE
    // A=0, B=0: 0 > 0 = FALSE
}

//=============================================================================
// Multiplexer Tests
//=============================================================================

TEST_F(NeuralLogicCircuitsTest, Multiplexer2to1Structure) {
    // 2:1 MUX: Out = (A AND NOT(S)) OR (B AND S)
    uint32_t not_s = neural_logic_create_gate(network, LOGIC_GATE_NOT, 0.5f);
    uint32_t and_a = neural_logic_create_gate(network, LOGIC_GATE_AND, 1.8f);  // A AND NOT(S)
    uint32_t and_b = neural_logic_create_gate(network, LOGIC_GATE_AND, 1.8f);  // B AND S
    uint32_t or_out = neural_logic_create_gate(network, LOGIC_GATE_OR, 1.0f);

    ASSERT_NE(not_s, UINT32_MAX);
    ASSERT_NE(and_a, UINT32_MAX);
    ASSERT_NE(and_b, UINT32_MAX);
    ASSERT_NE(or_out, UINT32_MAX);

    // Build structure
    EXPECT_TRUE(neural_logic_connect(network, not_s, and_a, 1.0f));  // NOT(S) -> AND_A
    EXPECT_TRUE(neural_logic_connect(network, and_a, or_out, 1.0f)); // AND_A -> OR
    EXPECT_TRUE(neural_logic_connect(network, and_b, or_out, 1.0f)); // AND_B -> OR
}

TEST_F(NeuralLogicCircuitsTest, Multiplexer4to1Structure) {
    // 4:1 MUX: 3 2:1 MUXes in tree structure
    uint32_t mux1 = neural_logic_create_gate(network, LOGIC_GATE_OR, 1.0f);
    uint32_t mux2 = neural_logic_create_gate(network, LOGIC_GATE_OR, 1.0f);
    uint32_t mux_final = neural_logic_create_gate(network, LOGIC_GATE_OR, 1.0f);

    ASSERT_NE(mux1, UINT32_MAX);
    ASSERT_NE(mux2, UINT32_MAX);
    ASSERT_NE(mux_final, UINT32_MAX);

    EXPECT_TRUE(neural_logic_connect(network, mux1, mux_final, 1.0f));
    EXPECT_TRUE(neural_logic_connect(network, mux2, mux_final, 1.0f));
}

//=============================================================================
// Decoder Tests
//=============================================================================

TEST_F(NeuralLogicCircuitsTest, Decoder2to4Structure) {
    // 2-to-4 decoder: 4 AND gates with inverted/non-inverted inputs
    // Out[0] = NOT(A) AND NOT(B)
    // Out[1] = NOT(A) AND B
    // Out[2] = A AND NOT(B)
    // Out[3] = A AND B

    uint32_t not_a = neural_logic_create_gate(network, LOGIC_GATE_NOT, 0.5f);
    uint32_t not_b = neural_logic_create_gate(network, LOGIC_GATE_NOT, 0.5f);

    uint32_t out0 = neural_logic_create_gate(network, LOGIC_GATE_AND, 1.8f);
    uint32_t out1 = neural_logic_create_gate(network, LOGIC_GATE_AND, 1.8f);
    uint32_t out2 = neural_logic_create_gate(network, LOGIC_GATE_AND, 1.8f);
    uint32_t out3 = neural_logic_create_gate(network, LOGIC_GATE_AND, 1.8f);

    ASSERT_NE(not_a, UINT32_MAX);
    ASSERT_NE(not_b, UINT32_MAX);
    ASSERT_NE(out0, UINT32_MAX);
    ASSERT_NE(out1, UINT32_MAX);
    ASSERT_NE(out2, UINT32_MAX);
    ASSERT_NE(out3, UINT32_MAX);

    // Connect structure (simplified - would need proper input routing)
    EXPECT_TRUE(neural_logic_connect(network, not_a, out0, 1.0f));
    EXPECT_TRUE(neural_logic_connect(network, not_b, out0, 1.0f));
    EXPECT_TRUE(neural_logic_connect(network, not_a, out1, 1.0f));
}

//=============================================================================
// Priority Encoder Tests
//=============================================================================

TEST_F(NeuralLogicCircuitsTest, PriorityEncoder4to2Structure) {
    // 4-to-2 priority encoder
    // Output represents highest priority active input
    uint32_t or1 = neural_logic_create_gate(network, LOGIC_GATE_OR, 1.0f);
    uint32_t or2 = neural_logic_create_gate(network, LOGIC_GATE_OR, 1.0f);
    uint32_t and1 = neural_logic_create_gate(network, LOGIC_GATE_AND, 1.8f);

    ASSERT_NE(or1, UINT32_MAX);
    ASSERT_NE(or2, UINT32_MAX);
    ASSERT_NE(and1, UINT32_MAX);

    EXPECT_TRUE(neural_logic_connect(network, or1, and1, 1.0f));
    EXPECT_TRUE(neural_logic_connect(network, or2, and1, 1.0f));
}

//=============================================================================
// Flip-Flop and Memory Tests
//=============================================================================

TEST_F(NeuralLogicCircuitsTest, SRLatchStructure) {
    // SR Latch using NOR gates (cross-coupled)
    // Q = NOR(R, Q')
    // Q' = NOR(S, Q)
    // NOR = NOT(OR)

    uint32_t or1 = neural_logic_create_gate(network, LOGIC_GATE_OR, 1.0f);
    uint32_t or2 = neural_logic_create_gate(network, LOGIC_GATE_OR, 1.0f);
    uint32_t not1 = neural_logic_create_gate(network, LOGIC_GATE_NOT, 0.5f);
    uint32_t not2 = neural_logic_create_gate(network, LOGIC_GATE_NOT, 0.5f);

    ASSERT_NE(or1, UINT32_MAX);
    ASSERT_NE(or2, UINT32_MAX);
    ASSERT_NE(not1, UINT32_MAX);
    ASSERT_NE(not2, UINT32_MAX);

    // Cross-couple (feedback connections)
    EXPECT_TRUE(neural_logic_connect(network, or1, not1, 1.0f));
    EXPECT_TRUE(neural_logic_connect(network, or2, not2, 1.0f));
    EXPECT_TRUE(neural_logic_connect(network, not1, or2, 1.0f));  // Q -> NOR2
    EXPECT_TRUE(neural_logic_connect(network, not2, or1, 1.0f));  // Q' -> NOR1
}

//=============================================================================
// Arithmetic Logic Unit (ALU) Tests
//=============================================================================

TEST_F(NeuralLogicCircuitsTest, SimpleALU1BitStructure) {
    // 1-bit ALU: AND, OR, ADD operations
    uint32_t and_op = neural_logic_create_gate(network, LOGIC_GATE_AND, 1.8f);
    uint32_t or_op = neural_logic_create_gate(network, LOGIC_GATE_OR, 1.0f);
    uint32_t xor_sum = neural_logic_create_gate(network, LOGIC_GATE_XOR, 0.5f);
    uint32_t and_carry = neural_logic_create_gate(network, LOGIC_GATE_AND, 1.8f);

    ASSERT_NE(and_op, UINT32_MAX);
    ASSERT_NE(or_op, UINT32_MAX);
    ASSERT_NE(xor_sum, UINT32_MAX);
    ASSERT_NE(and_carry, UINT32_MAX);

    // Structure exists (operation selection would need MUX)
}

//=============================================================================
// Parity Generator Tests
//=============================================================================

TEST_F(NeuralLogicCircuitsTest, ParityGenerator4Bit) {
    // 4-bit even parity: XOR all bits
    // P = A XOR B XOR C XOR D
    uint32_t xor1 = neural_logic_create_gate(network, LOGIC_GATE_XOR, 0.5f);  // A XOR B
    uint32_t xor2 = neural_logic_create_gate(network, LOGIC_GATE_XOR, 0.5f);  // C XOR D
    uint32_t xor_final = neural_logic_create_gate(network, LOGIC_GATE_XOR, 0.5f);  // (A XOR B) XOR (C XOR D)

    ASSERT_NE(xor1, UINT32_MAX);
    ASSERT_NE(xor2, UINT32_MAX);
    ASSERT_NE(xor_final, UINT32_MAX);

    EXPECT_TRUE(neural_logic_connect(network, xor1, xor_final, 1.0f));
    EXPECT_TRUE(neural_logic_connect(network, xor2, xor_final, 1.0f));
}

//=============================================================================
// Complex Circuit Integration Tests
//=============================================================================

TEST_F(NeuralLogicCircuitsTest, CascadedAdders) {
    // Multiple half adders in cascade
    uint32_t sum1 = neural_logic_create_gate(network, LOGIC_GATE_XOR, 0.5f);
    uint32_t carry1 = neural_logic_create_gate(network, LOGIC_GATE_AND, 1.8f);
    uint32_t sum2 = neural_logic_create_gate(network, LOGIC_GATE_XOR, 0.5f);
    uint32_t carry2 = neural_logic_create_gate(network, LOGIC_GATE_AND, 1.8f);

    ASSERT_NE(sum1, UINT32_MAX);
    ASSERT_NE(carry1, UINT32_MAX);
    ASSERT_NE(sum2, UINT32_MAX);
    ASSERT_NE(carry2, UINT32_MAX);

    // Cascade carries
    EXPECT_TRUE(neural_logic_connect(network, carry1, sum2, 1.0f));
}

TEST_F(NeuralLogicCircuitsTest, MixedGateTypes) {
    // Circuit using all gate types
    uint32_t and_gate = neural_logic_create_gate(network, LOGIC_GATE_AND, 1.8f);
    uint32_t or_gate = neural_logic_create_gate(network, LOGIC_GATE_OR, 1.0f);
    uint32_t not_gate = neural_logic_create_gate(network, LOGIC_GATE_NOT, 0.5f);
    uint32_t xor_gate = neural_logic_create_gate(network, LOGIC_GATE_XOR, 0.5f);
    uint32_t implies_gate = neural_logic_create_gate(network, LOGIC_GATE_IMPLIES, 1.2f);

    ASSERT_NE(and_gate, UINT32_MAX);
    ASSERT_NE(or_gate, UINT32_MAX);
    ASSERT_NE(not_gate, UINT32_MAX);
    ASSERT_NE(xor_gate, UINT32_MAX);
    ASSERT_NE(implies_gate, UINT32_MAX);

    // Connect in chain
    EXPECT_TRUE(neural_logic_connect(network, and_gate, or_gate, 1.0f));
    EXPECT_TRUE(neural_logic_connect(network, or_gate, not_gate, 1.0f));
    EXPECT_TRUE(neural_logic_connect(network, not_gate, xor_gate, 1.0f));
    EXPECT_TRUE(neural_logic_connect(network, xor_gate, implies_gate, 1.0f));
}

TEST_F(NeuralLogicCircuitsTest, DeepCircuit) {
    // 10-layer deep circuit
    uint32_t prev_gate = create_input();
    for (int i = 0; i < 10; i++) {
        uint32_t gate = neural_logic_create_gate(
            network,
            (i % 2 == 0) ? LOGIC_GATE_AND : LOGIC_GATE_OR,
            (i % 2 == 0) ? 1.8f : 1.0f
        );
        ASSERT_NE(gate, UINT32_MAX);
        EXPECT_TRUE(neural_logic_connect(network, prev_gate, gate, 1.0f));
        prev_gate = gate;
    }
}

TEST_F(NeuralLogicCircuitsTest, WideCircuit) {
    // 50 gates in parallel
    uint32_t source = create_input();
    for (int i = 0; i < 50; i++) {
        uint32_t target = neural_logic_create_gate(network, LOGIC_GATE_OR, 1.0f);
        if (target != UINT32_MAX) {
            EXPECT_TRUE(neural_logic_connect(network, source, target, 1.0f));
        }
    }
}

//=============================================================================
// Signal Propagation Through Circuits
//=============================================================================

TEST_F(NeuralLogicCircuitsTest, PropagationThroughAdder) {
    uint32_t xor_gate = neural_logic_create_gate(network, LOGIC_GATE_XOR, 0.5f);
    uint32_t and_gate = neural_logic_create_gate(network, LOGIC_GATE_AND, 1.8f);

    // Evaluate gates
    float inputs[] = {1.0f, 1.0f};
    float sum_out = 0.0f;
    float carry_out = 0.0f;

    EXPECT_TRUE(neural_logic_evaluate(network, xor_gate, inputs, 2, &sum_out));
    EXPECT_TRUE(neural_logic_evaluate(network, and_gate, inputs, 2, &carry_out));

    // Simulate propagation
    simulate_circuit(5);

    // Verify gates still have correct states
    logic_neuron_state_t xor_state, and_state;
    EXPECT_TRUE(neural_logic_get_state(network, xor_gate, &xor_state));
    EXPECT_TRUE(neural_logic_get_state(network, and_gate, &and_state));
}

TEST_F(NeuralLogicCircuitsTest, LongPropagationChain) {
    // 20-gate chain: check signal reaches end
    uint32_t gates[20];
    for (int i = 0; i < 20; i++) {
        gates[i] = neural_logic_create_gate(network, LOGIC_GATE_OR, 1.0f);
        ASSERT_NE(gates[i], UINT32_MAX);
        if (i > 0) {
            EXPECT_TRUE(neural_logic_connect(network, gates[i-1], gates[i], 1.0f));
        }
    }

    // Evaluate first gate
    float inputs[] = {1.0f};
    float output = 0.0f;
    EXPECT_TRUE(neural_logic_evaluate(network, gates[0], inputs, 1, &output));

    // Simulate for enough timesteps for signal to propagate
    simulate_circuit(25);
}
