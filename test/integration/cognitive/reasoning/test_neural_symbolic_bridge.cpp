//=============================================================================
// test_neural_symbolic_bridge.cpp - Neural-Symbolic Bridge Integration Tests
//=============================================================================

#include <gtest/gtest.h>
#include "core/brain/nimcp_brain.h"
#include "core/brain/learning/nimcp_circuit_compilation.h"

class NeuralSymbolicBridgeTest : public ::testing::Test {
protected:
    brain_t brain;

    void SetUp() override {
        // brain_config_t config;
        // config.num_neurons = 1000;
        // config.num_inputs = 10;
        // config.num_outputs = 5;
        // config.sparsity = 0.1f;
        // config.learning_rate = 0.01f;

        brain = brain_create("reasoning_test", BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION, 10, 5);
        ASSERT_NE(brain, nullptr);
    }

    void TearDown() override {
        if (brain) {
            brain_destroy(brain);
        }
    }
};

// Test 1: Compile simple rule to circuit
TEST_F(NeuralSymbolicBridgeTest, CompileSimpleRule) {
    const char* rule = "IF A AND B THEN C";
    circuit_id_t circuit_id = compile_rule_to_circuit(brain, rule);

    EXPECT_NE(circuit_id, 0);
}

// Test 2: Circuit gate count
TEST_F(NeuralSymbolicBridgeTest, CircuitGateCount) {
    const char* rule = "IF A AND B THEN C";
    circuit_id_t circuit_id = compile_rule_to_circuit(brain, rule);
    ASSERT_NE(circuit_id, 0);

    uint32_t gate_count = get_circuit_gate_count(brain, circuit_id);
    EXPECT_GT(gate_count, 0);
}

// Test 3: Circuit optimization
TEST_F(NeuralSymbolicBridgeTest, CircuitOptimization) {
    const char* rule = "IF A AND B THEN C";
    circuit_id_t circuit_id = compile_rule_to_circuit(brain, rule);
    ASSERT_NE(circuit_id, 0);

    bool optimized = optimize_circuit(brain, circuit_id);
    EXPECT_TRUE(optimized);
}

// Test 4: Circuit verification
TEST_F(NeuralSymbolicBridgeTest, CircuitVerification) {
    const char* rule = "IF A AND B THEN C";
    circuit_id_t circuit_id = compile_rule_to_circuit(brain, rule);
    ASSERT_NE(circuit_id, 0);

    // Create test cases
    const char* inputs1[] = {"A", "B"};
    const char* inputs2[] = {"A"};
    circuit_test_case_t tests[] = {
        {inputs1, 2, "C"},
        {inputs2, 1, ""}
    };

    bool verified = verify_circuit_correctness(brain, circuit_id, tests, 2);
    EXPECT_TRUE(verified);
}

// Test 5: Circuit deletion
TEST_F(NeuralSymbolicBridgeTest, CircuitDeletion) {
    const char* rule = "IF A THEN B";
    circuit_id_t circuit_id = compile_rule_to_circuit(brain, rule);
    ASSERT_NE(circuit_id, 0);

    bool deleted = delete_circuit(brain, circuit_id);
    EXPECT_TRUE(deleted);

    // Verify circuit is gone
    uint32_t gate_count = get_circuit_gate_count(brain, circuit_id);
    EXPECT_EQ(gate_count, 0);
}

// Test 6: Multiple circuits
TEST_F(NeuralSymbolicBridgeTest, MultipleCircuits) {
    circuit_id_t id1 = compile_rule_to_circuit(brain, "IF A THEN B");
    circuit_id_t id2 = compile_rule_to_circuit(brain, "IF C THEN D");
    circuit_id_t id3 = compile_rule_to_circuit(brain, "IF E THEN F");

    EXPECT_NE(id1, 0);
    EXPECT_NE(id2, 0);
    EXPECT_NE(id3, 0);
    EXPECT_NE(id1, id2);
    EXPECT_NE(id2, id3);
}

// Test 7: Circuit evaluation count
TEST_F(NeuralSymbolicBridgeTest, CircuitEvaluationCount) {
    const char* rule = "IF A THEN B";
    circuit_id_t circuit_id = compile_rule_to_circuit(brain, rule);
    ASSERT_NE(circuit_id, 0);

    uint64_t eval_count = get_circuit_eval_count(brain, circuit_id);
    EXPECT_EQ(eval_count, 0);  // Not evaluated yet
}

// Test 8: Invalid circuit operations
TEST_F(NeuralSymbolicBridgeTest, InvalidCircuitOperations) {
    circuit_id_t invalid_id = 9999;

    uint32_t gate_count = get_circuit_gate_count(brain, invalid_id);
    EXPECT_EQ(gate_count, 0);

    bool deleted = delete_circuit(brain, invalid_id);
    EXPECT_FALSE(deleted);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
