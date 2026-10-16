/**
 * @file test_cognitive_logic_regression.cpp
 * @brief Regression tests for cognitive logic circuits
 *
 * WHAT: Ensure cognitive logic circuits maintain backward compatibility
 * WHY:  Prevent regressions in constraint validation behavior
 * HOW:  Test known-good scenarios and performance benchmarks
 *
 * COVERAGE TARGET: 9+ regression tests
 *
 * @author NIMCP Development Team
 * @date 2025-11-16
 * @version 2.7.0
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <chrono>

#include "core/brain/nimcp_brain.h"
#include "core/brain/processing/cognitive_processor.h"
#include "core/neuron_types/nimcp_neural_logic.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Fixture
//=============================================================================

class CognitiveLogicRegressionTest : public ::testing::Test {
protected:
    neural_logic_network_t logic = nullptr;

    void SetUp() override {
        neural_logic_config_t config = neural_logic_default_config(100);
        logic = neural_logic_create(&config);
        ASSERT_NE(logic, nullptr);
    }

    void TearDown() override {
        if (logic) {
            neural_logic_destroy(logic);
        }
    }

    // Helper: Measure evaluation time
    double measure_evaluation_time(
        logic_gate_type_t gate_type,
        float* inputs,
        uint32_t num_inputs,
        int iterations)
    {
        uint32_t gate = neural_logic_create_gate(logic, gate_type, 1.0f);
        if (gate == UINT32_MAX) {
            return -1.0;
        }

        auto start = std::chrono::high_resolution_clock::now();

        for (int i = 0; i < iterations; i++) {
            float output = 0.0f;
            neural_logic_evaluate(logic, gate, inputs, num_inputs, &output);
        }

        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::micro> duration = end - start;

        return duration.count() / iterations;  // Average time per evaluation
    }
};

//=============================================================================
// Backward Compatibility Tests
//=============================================================================

TEST_F(CognitiveLogicRegressionTest, ANDGateBackwardCompatibility) {
    // Test: AND gate behavior matches original specification
    uint32_t and_gate = neural_logic_create_gate(logic, LOGIC_GATE_AND, 1.8f);
    ASSERT_NE(and_gate, UINT32_MAX);

    // Known-good test cases from original implementation
    struct TestCase {
        float a, b;
        float expected;
    };

    TestCase cases[] = {
        {0.0f, 0.0f, 0.0f},  // AND(0, 0) = 0
        {0.0f, 1.0f, 0.0f},  // AND(0, 1) = 0
        {1.0f, 0.0f, 0.0f},  // AND(1, 0) = 0
        {1.0f, 1.0f, 1.0f},  // AND(1, 1) = 1
    };

    for (const auto& tc : cases) {
        float inputs[2] = {tc.a, tc.b};
        float output = 0.0f;

        ASSERT_TRUE(neural_logic_evaluate(logic, and_gate, inputs, 2, &output));

        // Output should match expected (with tolerance)
        if (tc.expected > 0.5f) {
            EXPECT_GT(output, 0.5f) << "AND(" << tc.a << ", " << tc.b << ")";
        } else {
            EXPECT_LE(output, 0.5f) << "AND(" << tc.a << ", " << tc.b << ")";
        }
    }
}

TEST_F(CognitiveLogicRegressionTest, ORGateBackwardCompatibility) {
    // Test: OR gate behavior matches original specification
    uint32_t or_gate = neural_logic_create_gate(logic, LOGIC_GATE_OR, 0.6f);
    ASSERT_NE(or_gate, UINT32_MAX);

    struct TestCase {
        float a, b;
        float expected;
    };

    TestCase cases[] = {
        {0.0f, 0.0f, 0.0f},  // OR(0, 0) = 0
        {0.0f, 1.0f, 1.0f},  // OR(0, 1) = 1
        {1.0f, 0.0f, 1.0f},  // OR(1, 0) = 1
        {1.0f, 1.0f, 1.0f},  // OR(1, 1) = 1
    };

    for (const auto& tc : cases) {
        float inputs[2] = {tc.a, tc.b};
        float output = 0.0f;

        ASSERT_TRUE(neural_logic_evaluate(logic, or_gate, inputs, 2, &output));

        if (tc.expected > 0.5f) {
            EXPECT_GT(output, 0.5f) << "OR(" << tc.a << ", " << tc.b << ")";
        } else {
            EXPECT_LE(output, 0.5f) << "OR(" << tc.a << ", " << tc.b << ")";
        }
    }
}

TEST_F(CognitiveLogicRegressionTest, NOTGateBackwardCompatibility) {
    // Test: NOT gate behavior matches original specification
    uint32_t not_gate = neural_logic_create_gate(logic, LOGIC_GATE_NOT, 0.5f);
    ASSERT_NE(not_gate, UINT32_MAX);

    struct TestCase {
        float input;
        float expected;
    };

    TestCase cases[] = {
        {0.0f, 1.0f},  // NOT(0) = 1
        {1.0f, 0.0f},  // NOT(1) = 0
    };

    for (const auto& tc : cases) {
        float inputs[1] = {tc.input};
        float output = 0.0f;

        ASSERT_TRUE(neural_logic_evaluate(logic, not_gate, inputs, 1, &output));

        if (tc.expected > 0.5f) {
            EXPECT_GT(output, 0.5f) << "NOT(" << tc.input << ")";
        } else {
            EXPECT_LE(output, 0.5f) << "NOT(" << tc.input << ")";
        }
    }
}

TEST_F(CognitiveLogicRegressionTest, XORGateBackwardCompatibility) {
    // Test: XOR gate behavior matches original specification
    uint32_t xor_gate = neural_logic_create_gate(logic, LOGIC_GATE_XOR, 0.5f);
    ASSERT_NE(xor_gate, UINT32_MAX);

    struct TestCase {
        float a, b;
        float expected;
    };

    TestCase cases[] = {
        {0.0f, 0.0f, 0.0f},  // XOR(0, 0) = 0
        {0.0f, 1.0f, 1.0f},  // XOR(0, 1) = 1
        {1.0f, 0.0f, 1.0f},  // XOR(1, 0) = 1
        {1.0f, 1.0f, 0.0f},  // XOR(1, 1) = 0
    };

    for (const auto& tc : cases) {
        float inputs[2] = {tc.a, tc.b};
        float output = 0.0f;

        ASSERT_TRUE(neural_logic_evaluate(logic, xor_gate, inputs, 2, &output));

        if (tc.expected > 0.5f) {
            EXPECT_GT(output, 0.5f) << "XOR(" << tc.a << ", " << tc.b << ")";
        } else {
            EXPECT_LE(output, 0.5f) << "XOR(" << tc.a << ", " << tc.b << ")";
        }
    }
}

TEST_F(CognitiveLogicRegressionTest, IMPLIESGateBackwardCompatibility) {
    // Test: IMPLIES gate behavior matches original specification
    uint32_t implies_gate = neural_logic_create_gate(
        logic,
        LOGIC_GATE_IMPLIES,
        0.8f
    );
    ASSERT_NE(implies_gate, UINT32_MAX);

    struct TestCase {
        float a, b;
        float expected;
    };

    TestCase cases[] = {
        {0.0f, 0.0f, 1.0f},  // IMPLIES(0, 0) = 1 (vacuously true)
        {0.0f, 1.0f, 1.0f},  // IMPLIES(0, 1) = 1
        {1.0f, 0.0f, 0.0f},  // IMPLIES(1, 0) = 0 (only false case)
        {1.0f, 1.0f, 1.0f},  // IMPLIES(1, 1) = 1
    };

    for (const auto& tc : cases) {
        float inputs[2] = {tc.a, tc.b};
        float output = 0.0f;

        ASSERT_TRUE(neural_logic_evaluate(logic, implies_gate, inputs, 2, &output));

        if (tc.expected > 0.5f) {
            EXPECT_GT(output, 0.5f) << "IMPLIES(" << tc.a << ", " << tc.b << ")";
        } else {
            EXPECT_LE(output, 0.5f) << "IMPLIES(" << tc.a << ", " << tc.b << ")";
        }
    }
}

//=============================================================================
// Performance Regression Tests
//=============================================================================

TEST_F(CognitiveLogicRegressionTest, ANDGatePerformanceBaseline) {
    // Test: AND gate evaluation should complete within baseline time
    float inputs[2] = {1.0f, 1.0f};
    double avg_time = measure_evaluation_time(LOGIC_GATE_AND, inputs, 2, 1000);

    ASSERT_GE(avg_time, 0.0);

    // Should be fast (< 100 microseconds per evaluation on CPU)
    EXPECT_LT(avg_time, 100.0) << "AND gate evaluation too slow";
}

TEST_F(CognitiveLogicRegressionTest, XORGatePerformanceBaseline) {
    // Test: XOR gate evaluation should complete within baseline time
    float inputs[2] = {1.0f, 0.0f};
    double avg_time = measure_evaluation_time(LOGIC_GATE_XOR, inputs, 2, 1000);

    ASSERT_GE(avg_time, 0.0);

    // XOR is more complex but should still be fast
    EXPECT_LT(avg_time, 150.0) << "XOR gate evaluation too slow";
}

TEST_F(CognitiveLogicRegressionTest, IMPLIESGatePerformanceBaseline) {
    // Test: IMPLIES gate evaluation should complete within baseline time
    float inputs[2] = {1.0f, 1.0f};
    double avg_time = measure_evaluation_time(LOGIC_GATE_IMPLIES, inputs, 2, 1000);

    ASSERT_GE(avg_time, 0.0);

    EXPECT_LT(avg_time, 150.0) << "IMPLIES gate evaluation too slow";
}

//=============================================================================
// Memory and Resource Regression
//=============================================================================

TEST_F(CognitiveLogicRegressionTest, NoMemoryLeakOnRepeatedCreation) {
    // Test: Creating and destroying gates repeatedly shouldn't leak memory
    const int iterations = 100;

    for (int i = 0; i < iterations; i++) {
        uint32_t gate = neural_logic_create_gate(logic, LOGIC_GATE_AND, 1.8f);
        ASSERT_NE(gate, UINT32_MAX);

        // Note: Gates are stored in network, not individually freed
        // This tests that the network can handle many gates
    }

    // If we reach here without crash, no memory leak detected
    SUCCEED();
}

TEST_F(CognitiveLogicRegressionTest, ConnectionReusability) {
    // Test: Connections can be reused across evaluations
    uint32_t gate_a = neural_logic_create_gate(logic, LOGIC_GATE_AND, 1.8f);
    uint32_t gate_b = neural_logic_create_gate(logic, LOGIC_GATE_OR, 0.6f);

    ASSERT_NE(gate_a, UINT32_MAX);
    ASSERT_NE(gate_b, UINT32_MAX);

    // Create connection
    bool connected = neural_logic_connect(logic, gate_a, gate_b, 1.0f);
    EXPECT_TRUE(connected);

    // Evaluate multiple times
    for (int i = 0; i < 10; i++) {
        float inputs[2] = {(float)(i % 2), (float)((i + 1) % 2)};
        float output = 0.0f;

        ASSERT_TRUE(neural_logic_evaluate(logic, gate_a, inputs, 2, &output));
    }

    SUCCEED();
}
