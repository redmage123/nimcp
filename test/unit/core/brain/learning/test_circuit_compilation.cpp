//=============================================================================
// test_circuit_compilation.cpp - Circuit Compilation Unit Tests
//=============================================================================

#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <vector>

// Headers have their own extern "C" guards
#include "core/brain/learning/nimcp_circuit_compilation.h"
#include "core/brain/nimcp_brain.h"

//=============================================================================
// Test Fixture
//=============================================================================

class CircuitCompilationTest : public ::testing::Test {
protected:
    brain_t brain;

    void SetUp() override {
        brain = brain_create(100, 10);
        ASSERT_NE(brain, nullptr) << "Failed to create brain for testing";
    }

    void TearDown() override {
        if (brain) {
            brain_destroy(brain);
            brain = nullptr;
        }
    }
};

//=============================================================================
// Basic Compilation Tests
//=============================================================================

TEST_F(CircuitCompilationTest, CompileSimpleRule) {
    circuit_id_t circuit_id = compile_rule_to_circuit(
        brain, "IF A THEN B");

    EXPECT_NE(circuit_id, 0) << "Should successfully compile simple rule";

    // Cleanup
    delete_circuit(brain, circuit_id);
}

TEST_F(CircuitCompilationTest, CompileANDRule) {
    circuit_id_t circuit_id = compile_rule_to_circuit(
        brain, "IF (A AND B) THEN C");

    EXPECT_NE(circuit_id, 0) << "Should compile AND rule";

    uint32_t gate_count = get_circuit_gate_count(brain, circuit_id);
    EXPECT_GT(gate_count, 0) << "Circuit should have gates";

    delete_circuit(brain, circuit_id);
}

TEST_F(CircuitCompilationTest, CompileORRule) {
    circuit_id_t circuit_id = compile_rule_to_circuit(
        brain, "IF (A OR B) THEN C");

    EXPECT_NE(circuit_id, 0) << "Should compile OR rule";

    uint32_t gate_count = get_circuit_gate_count(brain, circuit_id);
    EXPECT_GT(gate_count, 0);

    delete_circuit(brain, circuit_id);
}

TEST_F(CircuitCompilationTest, CompileNOTRule) {
    circuit_id_t circuit_id = compile_rule_to_circuit(
        brain, "IF NOT A THEN B");

    EXPECT_NE(circuit_id, 0) << "Should compile NOT rule";

    delete_circuit(brain, circuit_id);
}

TEST_F(CircuitCompilationTest, CompileComplexRule) {
    circuit_id_t circuit_id = compile_rule_to_circuit(
        brain, "IF ((A AND B) OR (C AND D)) THEN E");

    EXPECT_NE(circuit_id, 0) << "Should compile complex nested rule";

    uint32_t gate_count = get_circuit_gate_count(brain, circuit_id);
    EXPECT_GT(gate_count, 3) << "Complex rule should have multiple gates";

    delete_circuit(brain, circuit_id);
}

//=============================================================================
// Multiple Circuit Tests
//=============================================================================

TEST_F(CircuitCompilationTest, CompileMultipleCircuits) {
    circuit_id_t id1 = compile_rule_to_circuit(brain, "IF A THEN B");
    circuit_id_t id2 = compile_rule_to_circuit(brain, "IF C THEN D");
    circuit_id_t id3 = compile_rule_to_circuit(brain, "IF E THEN F");

    EXPECT_NE(id1, 0);
    EXPECT_NE(id2, 0);
    EXPECT_NE(id3, 0);

    // All should have unique IDs
    EXPECT_NE(id1, id2);
    EXPECT_NE(id2, id3);
    EXPECT_NE(id1, id3);

    delete_circuit(brain, id1);
    delete_circuit(brain, id2);
    delete_circuit(brain, id3);
}

TEST_F(CircuitCompilationTest, DeleteAndRecompile) {
    circuit_id_t id1 = compile_rule_to_circuit(brain, "IF A THEN B");
    EXPECT_NE(id1, 0);

    bool deleted = delete_circuit(brain, id1);
    EXPECT_TRUE(deleted);

    // Recompile same rule
    circuit_id_t id2 = compile_rule_to_circuit(brain, "IF A THEN B");
    EXPECT_NE(id2, 0);

    delete_circuit(brain, id2);
}

//=============================================================================
// Circuit Gate Count Tests
//=============================================================================

TEST_F(CircuitCompilationTest, SimpleRuleGateCount) {
    circuit_id_t circuit_id = compile_rule_to_circuit(brain, "IF A THEN B");

    uint32_t count = get_circuit_gate_count(brain, circuit_id);

    // Simple rule should have at least input, output, and implication gate
    EXPECT_GE(count, 1);

    delete_circuit(brain, circuit_id);
}

TEST_F(CircuitCompilationTest, ComplexRuleGateCount) {
    circuit_id_t circuit_id = compile_rule_to_circuit(
        brain, "IF ((A AND B) OR (C AND D)) THEN E");

    uint32_t count = get_circuit_gate_count(brain, circuit_id);

    // Should have: 2 AND gates, 1 OR gate, 1 IMPLIES gate, plus inputs/outputs
    EXPECT_GE(count, 4);

    delete_circuit(brain, circuit_id);
}

TEST_F(CircuitCompilationTest, GateCountAfterOptimization) {
    circuit_id_t circuit_id = compile_rule_to_circuit(
        brain, "IF (A AND A) THEN B");

    uint32_t count_before = get_circuit_gate_count(brain, circuit_id);

    bool optimized = optimize_circuit(brain, circuit_id);
    EXPECT_TRUE(optimized);

    uint32_t count_after = get_circuit_gate_count(brain, circuit_id);

    // Optimization might reduce gates (A AND A = A)
    EXPECT_LE(count_after, count_before);

    delete_circuit(brain, circuit_id);
}

//=============================================================================
// Circuit Optimization Tests
//=============================================================================

TEST_F(CircuitCompilationTest, OptimizeSimpleCircuit) {
    circuit_id_t circuit_id = compile_rule_to_circuit(brain, "IF A THEN B");

    bool success = optimize_circuit(brain, circuit_id);
    EXPECT_TRUE(success);

    delete_circuit(brain, circuit_id);
}

TEST_F(CircuitCompilationTest, OptimizeComplexCircuit) {
    circuit_id_t circuit_id = compile_rule_to_circuit(
        brain, "IF ((A AND B) OR (C AND D)) THEN E");

    bool success = optimize_circuit(brain, circuit_id);
    EXPECT_TRUE(success);

    delete_circuit(brain, circuit_id);
}

TEST_F(CircuitCompilationTest, OptimizationPreservesFunction) {
    circuit_id_t circuit_id = compile_rule_to_circuit(brain, "IF A THEN B");

    // Create test case
    const char* inputs[] = {"A"};
    circuit_test_case_t test_case;
    test_case.inputs = inputs;
    test_case.num_inputs = 1;
    test_case.expected_output = "B";

    // Verify before optimization
    bool valid_before = verify_circuit_correctness(brain, circuit_id, &test_case, 1);

    optimize_circuit(brain, circuit_id);

    // Verify after optimization
    bool valid_after = verify_circuit_correctness(brain, circuit_id, &test_case, 1);

    // Function should be preserved
    EXPECT_EQ(valid_before, valid_after);

    delete_circuit(brain, circuit_id);
}

//=============================================================================
// Circuit Verification Tests
//=============================================================================

TEST_F(CircuitCompilationTest, VerifySimpleCircuit) {
    circuit_id_t circuit_id = compile_rule_to_circuit(brain, "IF A THEN B");

    const char* inputs[] = {"A"};
    circuit_test_case_t test_case;
    test_case.inputs = inputs;
    test_case.num_inputs = 1;
    test_case.expected_output = "B";

    bool verified = verify_circuit_correctness(brain, circuit_id, &test_case, 1);

    EXPECT_TRUE(verified) << "Simple circuit should verify correctly";

    delete_circuit(brain, circuit_id);
}

TEST_F(CircuitCompilationTest, VerifyANDCircuit) {
    circuit_id_t circuit_id = compile_rule_to_circuit(
        brain, "IF (A AND B) THEN C");

    // Test case: A=true, B=true -> C=true
    const char* inputs[] = {"A", "B"};
    circuit_test_case_t test_case;
    test_case.inputs = inputs;
    test_case.num_inputs = 2;
    test_case.expected_output = "C";

    bool verified = verify_circuit_correctness(brain, circuit_id, &test_case, 1);

    EXPECT_TRUE(verified);

    delete_circuit(brain, circuit_id);
}

TEST_F(CircuitCompilationTest, VerifyORCircuit) {
    circuit_id_t circuit_id = compile_rule_to_circuit(
        brain, "IF (A OR B) THEN C");

    // Test case 1: A=true -> C=true
    const char* inputs1[] = {"A"};
    circuit_test_case_t test_case1;
    test_case1.inputs = inputs1;
    test_case1.num_inputs = 1;
    test_case1.expected_output = "C";

    // Test case 2: B=true -> C=true
    const char* inputs2[] = {"B"};
    circuit_test_case_t test_case2;
    test_case2.inputs = inputs2;
    test_case2.num_inputs = 1;
    test_case2.expected_output = "C";

    circuit_test_case_t test_cases[] = {test_case1, test_case2};

    bool verified = verify_circuit_correctness(brain, circuit_id, test_cases, 2);

    EXPECT_TRUE(verified);

    delete_circuit(brain, circuit_id);
}

//=============================================================================
// Evaluation Count Tests
//=============================================================================

TEST_F(CircuitCompilationTest, InitialEvaluationCount) {
    circuit_id_t circuit_id = compile_rule_to_circuit(brain, "IF A THEN B");

    uint64_t eval_count = get_circuit_eval_count(brain, circuit_id);

    EXPECT_EQ(eval_count, 0) << "New circuit should have zero evaluations";

    delete_circuit(brain, circuit_id);
}

TEST_F(CircuitCompilationTest, EvaluationCountIncreases) {
    circuit_id_t circuit_id = compile_rule_to_circuit(brain, "IF A THEN B");

    const char* inputs[] = {"A"};
    circuit_test_case_t test_case;
    test_case.inputs = inputs;
    test_case.num_inputs = 1;
    test_case.expected_output = "B";

    // Verify multiple times
    for (int i = 0; i < 10; i++) {
        verify_circuit_correctness(brain, circuit_id, &test_case, 1);
    }

    uint64_t eval_count = get_circuit_eval_count(brain, circuit_id);

    EXPECT_GT(eval_count, 0) << "Evaluation count should increase";

    delete_circuit(brain, circuit_id);
}

//=============================================================================
// Delete Circuit Tests
//=============================================================================

TEST_F(CircuitCompilationTest, DeleteValidCircuit) {
    circuit_id_t circuit_id = compile_rule_to_circuit(brain, "IF A THEN B");
    EXPECT_NE(circuit_id, 0);

    bool deleted = delete_circuit(brain, circuit_id);
    EXPECT_TRUE(deleted);
}

TEST_F(CircuitCompilationTest, DeleteInvalidCircuit) {
    bool deleted = delete_circuit(brain, 99999);

    EXPECT_FALSE(deleted) << "Deleting non-existent circuit should fail";
}

TEST_F(CircuitCompilationTest, DoubleDeletion) {
    circuit_id_t circuit_id = compile_rule_to_circuit(brain, "IF A THEN B");

    bool deleted1 = delete_circuit(brain, circuit_id);
    EXPECT_TRUE(deleted1);

    bool deleted2 = delete_circuit(brain, circuit_id);
    EXPECT_FALSE(deleted2) << "Second deletion should fail";
}

//=============================================================================
// Edge Cases
//=============================================================================

TEST_F(CircuitCompilationTest, NullBrainCompile) {
    circuit_id_t circuit_id = compile_rule_to_circuit(nullptr, "IF A THEN B");

    EXPECT_EQ(circuit_id, 0) << "Should fail with null brain";
}

TEST_F(CircuitCompilationTest, NullRuleString) {
    circuit_id_t circuit_id = compile_rule_to_circuit(brain, nullptr);

    EXPECT_EQ(circuit_id, 0) << "Should fail with null rule string";
}

TEST_F(CircuitCompilationTest, EmptyRuleString) {
    circuit_id_t circuit_id = compile_rule_to_circuit(brain, "");

    EXPECT_EQ(circuit_id, 0) << "Should fail with empty rule string";
}

TEST_F(CircuitCompilationTest, InvalidRuleSyntax) {
    circuit_id_t circuit_id = compile_rule_to_circuit(brain, "INVALID SYNTAX");

    EXPECT_EQ(circuit_id, 0) << "Should fail with invalid syntax";
}

TEST_F(CircuitCompilationTest, GetGateCountInvalidCircuit) {
    uint32_t count = get_circuit_gate_count(brain, 99999);

    EXPECT_EQ(count, 0) << "Invalid circuit should return 0 gates";
}

TEST_F(CircuitCompilationTest, OptimizeInvalidCircuit) {
    bool success = optimize_circuit(brain, 99999);

    EXPECT_FALSE(success) << "Optimizing invalid circuit should fail";
}

TEST_F(CircuitCompilationTest, VerifyInvalidCircuit) {
    const char* inputs[] = {"A"};
    circuit_test_case_t test_case;
    test_case.inputs = inputs;
    test_case.num_inputs = 1;
    test_case.expected_output = "B";

    bool verified = verify_circuit_correctness(brain, 99999, &test_case, 1);

    EXPECT_FALSE(verified) << "Verifying invalid circuit should fail";
}

TEST_F(CircuitCompilationTest, GetEvalCountInvalidCircuit) {
    uint64_t count = get_circuit_eval_count(brain, 99999);

    EXPECT_EQ(count, 0) << "Invalid circuit should return 0 evaluations";
}

//=============================================================================
// Stress Tests
//=============================================================================

TEST_F(CircuitCompilationTest, CompileManyCircuits) {
    std::vector<circuit_id_t> circuits;

    for (int i = 0; i < 100; i++) {
        std::string rule = "IF X" + std::to_string(i) + " THEN Y" + std::to_string(i);
        circuit_id_t id = compile_rule_to_circuit(brain, rule.c_str());

        EXPECT_NE(id, 0) << "Failed to compile circuit " << i;
        circuits.push_back(id);
    }

    // Cleanup
    for (circuit_id_t id : circuits) {
        delete_circuit(brain, id);
    }
}

TEST_F(CircuitCompilationTest, DeepNesting) {
    circuit_id_t circuit_id = compile_rule_to_circuit(
        brain,
        "IF ((((A AND B) OR C) AND D) OR E) THEN F"
    );

    EXPECT_NE(circuit_id, 0) << "Should handle deep nesting";

    uint32_t gate_count = get_circuit_gate_count(brain, circuit_id);
    EXPECT_GT(gate_count, 5) << "Deeply nested rule should have many gates";

    delete_circuit(brain, circuit_id);
}

TEST_F(CircuitCompilationTest, WideCircuit) {
    circuit_id_t circuit_id = compile_rule_to_circuit(
        brain,
        "IF (A OR B OR C OR D OR E OR F OR G OR H) THEN Z"
    );

    EXPECT_NE(circuit_id, 0) << "Should handle wide OR gates";

    delete_circuit(brain, circuit_id);
}

//=============================================================================
// Logical Equivalence Tests
//=============================================================================

TEST_F(CircuitCompilationTest, DeMorgansLaw) {
    // NOT(A AND B) = (NOT A) OR (NOT B)
    circuit_id_t id1 = compile_rule_to_circuit(
        brain, "IF NOT (A AND B) THEN C");
    circuit_id_t id2 = compile_rule_to_circuit(
        brain, "IF ((NOT A) OR (NOT B)) THEN C");

    EXPECT_NE(id1, 0);
    EXPECT_NE(id2, 0);

    // Both circuits should verify with same test cases
    const char* inputs[] = {"A"};
    circuit_test_case_t test_case;
    test_case.inputs = inputs;
    test_case.num_inputs = 1;
    test_case.expected_output = "C";

    bool verified1 = verify_circuit_correctness(brain, id1, &test_case, 1);
    bool verified2 = verify_circuit_correctness(brain, id2, &test_case, 1);

    // Both should have same verification result (logically equivalent)
    EXPECT_EQ(verified1, verified2);

    delete_circuit(brain, id1);
    delete_circuit(brain, id2);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
