/**
 * @file test_neural_logic_circuit_builder.cpp
 * @brief Unit Tests for MODULE 3: Neural Logic Circuit Builder
 * @version 3.0.0
 * @date 2025-11-20
 *
 * TEST COVERAGE: 12 tests, 100% function coverage
 * - parse_logic_expression: 5 tests
 * - build_circuit_from_ast: 3 tests
 * - brain_build_logic_circuit: 2 tests
 * - destroy_circuit: 1 test
 * - free_ast: 1 test
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "core/logic/nimcp_neural_logic_circuit_builder.h"
#include "core/logic/nimcp_neural_logic_factory.h"
#include "core/brain/nimcp_brain.h"

class NeuralLogicCircuitBuilderTest : public ::testing::Test {
protected:
    brain_t brain;

    void SetUp() override {
        brain = brain_create("circuit_test_brain", BRAIN_SIZE_SMALL,
                              BRAIN_TASK_CLASSIFICATION, 10, 5);
        ASSERT_NE(brain, nullptr);

        bool attached = create_and_attach_neural_logic(brain, BRAIN_SIZE_SMALL);
        ASSERT_TRUE(attached);
    }

    void TearDown() override {
        if (brain) {
            brain_destroy(brain);
        }
    }
};

//=============================================================================
// Test: parse_logic_expression
//=============================================================================

TEST_F(NeuralLogicCircuitBuilderTest, ParseSimpleVariable) {
    ast_node_t* ast = parse_logic_expression("A");

    ASSERT_NE(ast, nullptr);
    EXPECT_EQ(ast->type, AST_NODE_VARIABLE);
    EXPECT_EQ(ast->data.variable.name, 'A');

    free_ast(ast);
}

TEST_F(NeuralLogicCircuitBuilderTest, ParseAndExpression) {
    ast_node_t* ast = parse_logic_expression("A AND B");

    ASSERT_NE(ast, nullptr);
    EXPECT_EQ(ast->type, AST_NODE_OPERATOR);
    EXPECT_EQ(ast->data.op.gate_type, LOGIC_GATE_AND);
    ASSERT_NE(ast->data.op.left, nullptr);
    ASSERT_NE(ast->data.op.right, nullptr);

    free_ast(ast);
}

TEST_F(NeuralLogicCircuitBuilderTest, ParseNotExpression) {
    ast_node_t* ast = parse_logic_expression("NOT A");

    ASSERT_NE(ast, nullptr);
    EXPECT_EQ(ast->type, AST_NODE_OPERATOR);
    EXPECT_EQ(ast->data.op.gate_type, LOGIC_GATE_NOT);
    ASSERT_NE(ast->data.op.left, nullptr);
    EXPECT_EQ(ast->data.op.right, nullptr);

    free_ast(ast);
}

TEST_F(NeuralLogicCircuitBuilderTest, ParseNullExpression) {
    ast_node_t* ast = parse_logic_expression(nullptr);

    EXPECT_EQ(ast, nullptr);
}

TEST_F(NeuralLogicCircuitBuilderTest, ParseEmptyExpression) {
    ast_node_t* ast = parse_logic_expression("");

    EXPECT_EQ(ast, nullptr);
}

//=============================================================================
// Test: build_circuit_from_ast
//=============================================================================

TEST_F(NeuralLogicCircuitBuilderTest, BuildCircuitFromVariableAST) {
    ast_node_t* ast = parse_logic_expression("A");
    ASSERT_NE(ast, nullptr);

    uint32_t circuit_id = build_circuit_from_ast(brain, ast);

    EXPECT_NE(circuit_id, UINT32_MAX);

    free_ast(ast);
}

TEST_F(NeuralLogicCircuitBuilderTest, BuildCircuitNullBrain) {
    ast_node_t* ast = parse_logic_expression("A");
    ASSERT_NE(ast, nullptr);

    uint32_t circuit_id = build_circuit_from_ast(nullptr, ast);

    EXPECT_EQ(circuit_id, UINT32_MAX);

    free_ast(ast);
}

TEST_F(NeuralLogicCircuitBuilderTest, BuildCircuitNullAST) {
    uint32_t circuit_id = build_circuit_from_ast(brain, nullptr);

    EXPECT_EQ(circuit_id, UINT32_MAX);
}

//=============================================================================
// Test: brain_build_logic_circuit
//=============================================================================

TEST_F(NeuralLogicCircuitBuilderTest, BuildCircuitSuccess) {
    uint32_t circuit_id = brain_build_logic_circuit(brain, "A AND B");

    EXPECT_NE(circuit_id, UINT32_MAX);
}

TEST_F(NeuralLogicCircuitBuilderTest, BuildCircuitNullExpression) {
    uint32_t circuit_id = brain_build_logic_circuit(brain, nullptr);

    EXPECT_EQ(circuit_id, UINT32_MAX);
}

//=============================================================================
// Test: destroy_circuit
//=============================================================================

TEST_F(NeuralLogicCircuitBuilderTest, DestroyCircuitSuccess) {
    uint32_t circuit_id = brain_build_logic_circuit(brain, "A");
    ASSERT_NE(circuit_id, UINT32_MAX);

    bool result = destroy_circuit(brain, circuit_id);

    // Placeholder implementation always succeeds
    EXPECT_TRUE(result);
}

//=============================================================================
// Test: free_ast
//=============================================================================

TEST_F(NeuralLogicCircuitBuilderTest, FreeASTNullSafe) {
    // Should not crash
    free_ast(nullptr);

    SUCCEED();
}
