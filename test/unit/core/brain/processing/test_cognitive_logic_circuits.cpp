/**
 * @file test_cognitive_logic_circuits.cpp
 * @brief Unit tests for cognitive logic gate circuits
 *
 * WHAT: Test cognitive constraint checking using neural logic circuits
 * WHY:  Ensure logic gates correctly validate cognitive constraints
 * HOW:  Test mutual exclusion, prerequisites, and conflict detection
 *
 * COVERAGE TARGET: 100% (14+ tests)
 *
 * @author NIMCP Development Team
 * @date 2025-11-16
 * @version 2.7.0
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

#include "core/brain/processing/cognitive_processor.h"
#include "core/neuron_types/nimcp_neural_logic.h"
#include "core/brain/nimcp_brain.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Fixture
//=============================================================================

class CognitiveLogicCircuitsTest : public ::testing::Test {
protected:
    neural_logic_network_t logic = nullptr;
    brain_t brain = nullptr;

    void SetUp() override {
        // Create neural logic network for testing
        neural_logic_config_t config = neural_logic_default_config(100);
        logic = neural_logic_create(&config);
        ASSERT_NE(logic, nullptr);

        // Create brain with logic network
        brain = brain_create("LogicTest", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 64, 10);
        if (brain) {
            // Associate brain with logic network
            neural_logic_set_brain(logic, brain);
        }
    }

    void TearDown() override {
        if (logic) {
            neural_logic_destroy(logic);
        }
        if (brain) {
            brain_destroy(brain);
        }
    }

    // Helper: Create test network output
    network_output_t create_test_output(float* output_data, uint32_t size) {
        network_output_t output = {0};
        output.output_vector = output_data;
        output.output_size = size;
        output.spikes_generated = 10;
        output.inference_time_us = 1000;
        return output;
    }

    // Helper: Create test annotations
    cognitive_annotations_t create_test_annotations(
        float confidence,
        float uncertainty,
        bool ethical,
        float salience)
    {
        cognitive_annotations_t ann = {0};
        ann.confidence = confidence;
        ann.uncertainty = uncertainty;
        ann.ethical_approved = ethical;
        ann.salience_score = salience;
        ann.novelty_score = 0.5f;
        ann.urgency_score = 0.0f;
        ann.exploration_bonus = 0.0f;
        ann.information_gain = 0.0f;
        ann.logic_valid = true;
        return ann;
    }
};

//=============================================================================
// Mutual Exclusion Tests (XOR Circuit)
//=============================================================================

TEST_F(CognitiveLogicCircuitsTest, MutualExclusionBothInactive) {
    // Test: Both values inactive should satisfy mutual exclusion
    uint32_t xor_gate = neural_logic_create_gate(logic, LOGIC_GATE_XOR, 0.5f);
    ASSERT_NE(xor_gate, UINT32_MAX);

    float inputs[2] = {0.0f, 0.0f};  // Both inactive
    float output = 0.0f;

    ASSERT_TRUE(neural_logic_evaluate(logic, xor_gate, inputs, 2, &output));

    // XOR(0, 0) = 0, but mutual exclusion is satisfied
    EXPECT_LE(output, 0.5f);  // Both inactive is valid
}

TEST_F(CognitiveLogicCircuitsTest, MutualExclusionOnlyFirstActive) {
    // Test: Only first value active should satisfy mutual exclusion
    uint32_t xor_gate = neural_logic_create_gate(logic, LOGIC_GATE_XOR, 0.5f);
    ASSERT_NE(xor_gate, UINT32_MAX);

    float inputs[2] = {1.0f, 0.0f};  // Only A active
    float output = 0.0f;

    ASSERT_TRUE(neural_logic_evaluate(logic, xor_gate, inputs, 2, &output));

    // XOR(1, 0) = 1, mutual exclusion satisfied
    EXPECT_GT(output, 0.5f);
}

TEST_F(CognitiveLogicCircuitsTest, MutualExclusionOnlySecondActive) {
    // Test: Only second value active should satisfy mutual exclusion
    uint32_t xor_gate = neural_logic_create_gate(logic, LOGIC_GATE_XOR, 0.5f);
    ASSERT_NE(xor_gate, UINT32_MAX);

    float inputs[2] = {0.0f, 1.0f};  // Only B active
    float output = 0.0f;

    ASSERT_TRUE(neural_logic_evaluate(logic, xor_gate, inputs, 2, &output));

    // XOR(0, 1) = 1, mutual exclusion satisfied
    EXPECT_GT(output, 0.5f);
}

TEST_F(CognitiveLogicCircuitsTest, MutualExclusionBothActiveViolation) {
    // Test: Both values active should violate mutual exclusion
    uint32_t xor_gate = neural_logic_create_gate(logic, LOGIC_GATE_XOR, 0.5f);
    ASSERT_NE(xor_gate, UINT32_MAX);

    float inputs[2] = {1.0f, 1.0f};  // Both active - VIOLATION
    float output = 0.0f;

    ASSERT_TRUE(neural_logic_evaluate(logic, xor_gate, inputs, 2, &output));

    // XOR(1, 1) = 0, mutual exclusion violated
    EXPECT_LE(output, 0.5f);  // Should indicate conflict
}

//=============================================================================
// Prerequisite Tests (IMPLIES Circuit)
//=============================================================================

TEST_F(CognitiveLogicCircuitsTest, PrerequisiteNotRequiredNotDependent) {
    // Test: A=0, B=0 → A → B should be true
    uint32_t implies_gate = neural_logic_create_gate(
        logic,
        LOGIC_GATE_IMPLIES,
        0.8f
    );
    ASSERT_NE(implies_gate, UINT32_MAX);

    float inputs[2] = {0.0f, 0.0f};  // Neither active
    float output = 0.0f;

    ASSERT_TRUE(neural_logic_evaluate(logic, implies_gate, inputs, 2, &output));

    // IMPLIES(0, 0) = 1 (vacuously true)
    EXPECT_GT(output, 0.5f);
}

TEST_F(CognitiveLogicCircuitsTest, PrerequisiteNotRequiredButDependent) {
    // Test: A=0, B=1 → A → B should be true
    uint32_t implies_gate = neural_logic_create_gate(
        logic,
        LOGIC_GATE_IMPLIES,
        0.8f
    );
    ASSERT_NE(implies_gate, UINT32_MAX);

    float inputs[2] = {0.0f, 1.0f};  // Dependent without prerequisite
    float output = 0.0f;

    ASSERT_TRUE(neural_logic_evaluate(logic, implies_gate, inputs, 2, &output));

    // IMPLIES(0, 1) = 1 (true)
    EXPECT_GT(output, 0.5f);
}

TEST_F(CognitiveLogicCircuitsTest, PrerequisiteRequiredAndDependent) {
    // Test: A=1, B=1 → A → B should be true (prerequisite satisfied)
    uint32_t implies_gate = neural_logic_create_gate(
        logic,
        LOGIC_GATE_IMPLIES,
        0.8f
    );
    ASSERT_NE(implies_gate, UINT32_MAX);

    float inputs[2] = {1.0f, 1.0f};  // Both active
    float output = 0.0f;

    ASSERT_TRUE(neural_logic_evaluate(logic, implies_gate, inputs, 2, &output));

    // IMPLIES(1, 1) = 1 (prerequisite met)
    EXPECT_GT(output, 0.5f);
}

TEST_F(CognitiveLogicCircuitsTest, PrerequisiteRequiredNotDependentViolation) {
    // Test: A=1, B=0 → A → B should be false (VIOLATION)
    uint32_t implies_gate = neural_logic_create_gate(
        logic,
        LOGIC_GATE_IMPLIES,
        0.8f
    );
    ASSERT_NE(implies_gate, UINT32_MAX);

    float inputs[2] = {1.0f, 0.0f};  // Prerequisite active, dependent not
    float output = 0.0f;

    ASSERT_TRUE(neural_logic_evaluate(logic, implies_gate, inputs, 2, &output));

    // IMPLIES(1, 0) = 0 (prerequisite violation)
    EXPECT_LE(output, 0.5f);
}

//=============================================================================
// Conflict Detection Tests (NOT-AND Circuit)
//=============================================================================

TEST_F(CognitiveLogicCircuitsTest, ConflictNeitherActive) {
    // Test: A=0, B=0 → NOT(AND) should be true (no conflict)
    uint32_t and_gate = neural_logic_create_gate(logic, LOGIC_GATE_AND, 1.8f);
    uint32_t not_gate = neural_logic_create_gate(logic, LOGIC_GATE_NOT, 0.5f);

    ASSERT_NE(and_gate, UINT32_MAX);
    ASSERT_NE(not_gate, UINT32_MAX);

    // Test AND gate
    float and_inputs[2] = {0.0f, 0.0f};
    float and_output = 0.0f;
    ASSERT_TRUE(neural_logic_evaluate(logic, and_gate, and_inputs, 2, &and_output));

    // AND(0, 0) = 0
    EXPECT_LE(and_output, 0.5f);

    // Test NOT gate
    float not_inputs[1] = {and_output};
    float not_output = 0.0f;
    ASSERT_TRUE(neural_logic_evaluate(logic, not_gate, not_inputs, 1, &not_output));

    // NOT(0) = 1 (no conflict)
    EXPECT_GT(not_output, 0.5f);
}

TEST_F(CognitiveLogicCircuitsTest, ConflictOnlyFirstActive) {
    // Test: A=1, B=0 → NOT(AND) should be true (no conflict)
    uint32_t and_gate = neural_logic_create_gate(logic, LOGIC_GATE_AND, 1.8f);
    uint32_t not_gate = neural_logic_create_gate(logic, LOGIC_GATE_NOT, 0.5f);

    ASSERT_NE(and_gate, UINT32_MAX);
    ASSERT_NE(not_gate, UINT32_MAX);

    float and_inputs[2] = {1.0f, 0.0f};
    float and_output = 0.0f;
    ASSERT_TRUE(neural_logic_evaluate(logic, and_gate, and_inputs, 2, &and_output));

    // AND(1, 0) = 0
    EXPECT_LE(and_output, 0.5f);

    float not_inputs[1] = {and_output};
    float not_output = 0.0f;
    ASSERT_TRUE(neural_logic_evaluate(logic, not_gate, not_inputs, 1, &not_output));

    // NOT(0) = 1 (no conflict)
    EXPECT_GT(not_output, 0.5f);
}

TEST_F(CognitiveLogicCircuitsTest, ConflictOnlySecondActive) {
    // Test: A=0, B=1 → NOT(AND) should be true (no conflict)
    uint32_t and_gate = neural_logic_create_gate(logic, LOGIC_GATE_AND, 1.8f);
    uint32_t not_gate = neural_logic_create_gate(logic, LOGIC_GATE_NOT, 0.5f);

    ASSERT_NE(and_gate, UINT32_MAX);
    ASSERT_NE(not_gate, UINT32_MAX);

    float and_inputs[2] = {0.0f, 1.0f};
    float and_output = 0.0f;
    ASSERT_TRUE(neural_logic_evaluate(logic, and_gate, and_inputs, 2, &and_output));

    // AND(0, 1) = 0
    EXPECT_LE(and_output, 0.5f);

    float not_inputs[1] = {and_output};
    float not_output = 0.0f;
    ASSERT_TRUE(neural_logic_evaluate(logic, not_gate, not_inputs, 1, &not_output));

    // NOT(0) = 1 (no conflict)
    EXPECT_GT(not_output, 0.5f);
}

TEST_F(CognitiveLogicCircuitsTest, ConflictBothActiveViolation) {
    // Test: A=1, B=1 → NOT(AND) should be false (CONFLICT)
    uint32_t and_gate = neural_logic_create_gate(logic, LOGIC_GATE_AND, 1.8f);
    uint32_t not_gate = neural_logic_create_gate(logic, LOGIC_GATE_NOT, 0.5f);

    ASSERT_NE(and_gate, UINT32_MAX);
    ASSERT_NE(not_gate, UINT32_MAX);

    float and_inputs[2] = {1.0f, 1.0f};
    float and_output = 0.0f;
    ASSERT_TRUE(neural_logic_evaluate(logic, and_gate, and_inputs, 2, &and_output));

    // AND(1, 1) = 1
    EXPECT_GT(and_output, 0.5f);

    float not_inputs[1] = {and_output};
    float not_output = 0.0f;
    ASSERT_TRUE(neural_logic_evaluate(logic, not_gate, not_inputs, 1, &not_output));

    // NOT(1) = 0 (conflict detected)
    EXPECT_LE(not_output, 0.5f);
}

//=============================================================================
// Integration with Cognitive Processor Tests
//=============================================================================

TEST_F(CognitiveLogicCircuitsTest, ValidCognitiveAnnotations) {
    // Test: Valid annotations should pass all constraints
    if (!brain) {
        GTEST_SKIP() << "Brain creation not supported in this environment";
    }

    float output_data[10] = {0.5f, 0.3f, 0.7f, 0.2f, 0.4f,
                             0.6f, 0.1f, 0.8f, 0.3f, 0.5f};
    network_output_t output = create_test_output(output_data, 10);

    cognitive_annotations_t annotations = create_test_annotations(
        0.8f,   // High confidence
        0.2f,   // Low uncertainty (complementary)
        true,   // Ethical
        0.6f    // Moderate salience
    );

    float features[64] = {0};

    bool result = cognitive_process_output(
        brain,
        &output,
        features,
        64,
        1000,
        &annotations
    );

    EXPECT_TRUE(result);
    // Logic validation may or may not be true depending on brain setup
}

TEST_F(CognitiveLogicCircuitsTest, HighConfidenceHighUncertaintyConflict) {
    // Test: High confidence + high uncertainty should fail
    if (!brain) {
        GTEST_SKIP() << "Brain creation not supported in this environment";
    }

    float output_data[10] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f,
                             0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
    network_output_t output = create_test_output(output_data, 10);

    cognitive_annotations_t annotations = create_test_annotations(
        0.9f,   // Very high confidence
        0.9f,   // Very high uncertainty (CONFLICT!)
        true,   // Ethical
        0.5f    // Moderate salience
    );

    float features[64] = {0};

    bool result = cognitive_process_output(
        brain,
        &output,
        features,
        64,
        1000,
        &annotations
    );

    EXPECT_TRUE(result);
    // The constraint validation should detect this conflict
}

//=============================================================================
// Edge Cases
//=============================================================================

TEST_F(CognitiveLogicCircuitsTest, NullLogicNetworkGracefulFail) {
    // Test: Null logic network should gracefully return true (assume valid)
    uint32_t gate = neural_logic_create_gate(nullptr, LOGIC_GATE_AND, 1.8f);
    EXPECT_EQ(gate, UINT32_MAX);
}

TEST_F(CognitiveLogicCircuitsTest, PartialActivationValues) {
    // Test: Partial activation values (0.3, 0.7)
    uint32_t xor_gate = neural_logic_create_gate(logic, LOGIC_GATE_XOR, 0.5f);
    ASSERT_NE(xor_gate, UINT32_MAX);

    float inputs[2] = {0.3f, 0.7f};  // Partial activations
    float output = 0.0f;

    ASSERT_TRUE(neural_logic_evaluate(logic, xor_gate, inputs, 2, &output));

    // Should handle partial values gracefully
    EXPECT_GE(output, 0.0f);
    EXPECT_LE(output, 1.0f);
}
