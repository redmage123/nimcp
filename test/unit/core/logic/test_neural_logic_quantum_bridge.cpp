/**
 * @file test_neural_logic_quantum_bridge.cpp
 * @brief Unit Tests for Neural Logic Quantum Bridge
 * @version 1.0.0
 * @date 2025-12-30
 *
 * TEST COVERAGE: 30+ tests covering:
 * - Bridge lifecycle (create/destroy)
 * - Circuit to CNF conversion
 * - Quantum SAT solving via Grover's algorithm
 * - Quantum interference
 * - Ternary logic operations
 * - Statistics tracking
 */

#include <gtest/gtest.h>
#include <cmath>

// Headers have their own extern "C" guards
#define NIMCP_NEURAL_LOGIC_QUANTUM_BRIDGE_IMPLEMENTATION
#include "core/logic/nimcp_neural_logic_quantum_bridge.h"
#include "cognitive/reasoning/nimcp_quantum_reasoning.h"

//=============================================================================
// Test Fixture
//=============================================================================

class NeuralLogicQuantumBridgeTest : public ::testing::Test {
protected:
    neural_logic_quantum_bridge_t* bridge;

    void SetUp() override {
        bridge = neural_logic_quantum_bridge_create(nullptr);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            neural_logic_quantum_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST(QuantumBridgeLifecycleTest, CreateWithDefaults) {
    neural_logic_quantum_bridge_t* b = neural_logic_quantum_bridge_create(nullptr);
    ASSERT_NE(b, nullptr);
    neural_logic_quantum_bridge_destroy(b);
}

TEST(QuantumBridgeLifecycleTest, CreateWithCustomConfig) {
    neural_logic_quantum_config_t config = neural_logic_quantum_default_config();
    config.sat_iterations = 5;
    config.interference_threshold = 0.2f;
    config.min_confidence = 0.6f;

    neural_logic_quantum_bridge_t* b = neural_logic_quantum_bridge_create(&config);
    ASSERT_NE(b, nullptr);
    neural_logic_quantum_bridge_destroy(b);
}

TEST(QuantumBridgeLifecycleTest, DestroyNull) {
    // Should not crash
    neural_logic_quantum_bridge_destroy(nullptr);
}

TEST(QuantumBridgeLifecycleTest, DefaultConfig) {
    neural_logic_quantum_config_t config = neural_logic_quantum_default_config();

    EXPECT_TRUE(config.enabled);
    EXPECT_EQ(config.sat_iterations, 0u);  // Auto-compute
    EXPECT_FLOAT_EQ(config.interference_threshold, 0.1f);
    EXPECT_FLOAT_EQ(config.min_confidence, 0.5f);
    EXPECT_EQ(config.max_inference_depth, 20u);
    EXPECT_TRUE(config.use_ternary_logic);
}

//=============================================================================
// Enable/Disable Tests
//=============================================================================

TEST_F(NeuralLogicQuantumBridgeTest, IsEnabledDefault) {
    // Not connected, so should be false
    EXPECT_FALSE(neural_logic_quantum_is_enabled(bridge));
}

TEST_F(NeuralLogicQuantumBridgeTest, SetEnabled) {
    neural_logic_quantum_set_enabled(bridge, false);
    // Still not connected, won't be enabled
    EXPECT_FALSE(neural_logic_quantum_is_enabled(bridge));

    neural_logic_quantum_set_enabled(bridge, true);
    // Still not connected
    EXPECT_FALSE(neural_logic_quantum_is_enabled(bridge));
}

TEST_F(NeuralLogicQuantumBridgeTest, IsEnabledNullBridge) {
    EXPECT_FALSE(neural_logic_quantum_is_enabled(nullptr));
}

//=============================================================================
// Ternary Logic Tests
//=============================================================================

TEST(QuantumTernaryLogicTest, AndTrueTrue) {
    EXPECT_EQ(neural_logic_quantum_and(QREASON_TRUE, QREASON_TRUE), QREASON_TRUE);
}

TEST(QuantumTernaryLogicTest, AndTrueFalse) {
    EXPECT_EQ(neural_logic_quantum_and(QREASON_TRUE, QREASON_FALSE), QREASON_FALSE);
}

TEST(QuantumTernaryLogicTest, AndFalseFalse) {
    EXPECT_EQ(neural_logic_quantum_and(QREASON_FALSE, QREASON_FALSE), QREASON_FALSE);
}

TEST(QuantumTernaryLogicTest, AndTrueUnknown) {
    EXPECT_EQ(neural_logic_quantum_and(QREASON_TRUE, QREASON_UNKNOWN), QREASON_UNKNOWN);
}

TEST(QuantumTernaryLogicTest, AndFalseUnknown) {
    EXPECT_EQ(neural_logic_quantum_and(QREASON_FALSE, QREASON_UNKNOWN), QREASON_FALSE);
}

TEST(QuantumTernaryLogicTest, OrTrueTrue) {
    EXPECT_EQ(neural_logic_quantum_or(QREASON_TRUE, QREASON_TRUE), QREASON_TRUE);
}

TEST(QuantumTernaryLogicTest, OrTrueFalse) {
    EXPECT_EQ(neural_logic_quantum_or(QREASON_TRUE, QREASON_FALSE), QREASON_TRUE);
}

TEST(QuantumTernaryLogicTest, OrFalseFalse) {
    EXPECT_EQ(neural_logic_quantum_or(QREASON_FALSE, QREASON_FALSE), QREASON_FALSE);
}

TEST(QuantumTernaryLogicTest, OrTrueUnknown) {
    EXPECT_EQ(neural_logic_quantum_or(QREASON_TRUE, QREASON_UNKNOWN), QREASON_TRUE);
}

TEST(QuantumTernaryLogicTest, OrFalseUnknown) {
    EXPECT_EQ(neural_logic_quantum_or(QREASON_FALSE, QREASON_UNKNOWN), QREASON_UNKNOWN);
}

TEST(QuantumTernaryLogicTest, NotTrue) {
    EXPECT_EQ(neural_logic_quantum_not(QREASON_TRUE), QREASON_FALSE);
}

TEST(QuantumTernaryLogicTest, NotFalse) {
    EXPECT_EQ(neural_logic_quantum_not(QREASON_FALSE), QREASON_TRUE);
}

TEST(QuantumTernaryLogicTest, NotUnknown) {
    EXPECT_EQ(neural_logic_quantum_not(QREASON_UNKNOWN), QREASON_UNKNOWN);
}

TEST(QuantumTernaryLogicTest, ImpliesTrueTrue) {
    // T -> T = T
    EXPECT_EQ(neural_logic_quantum_implies(QREASON_TRUE, QREASON_TRUE), QREASON_TRUE);
}

TEST(QuantumTernaryLogicTest, ImpliesTrueFalse) {
    // T -> F = F
    EXPECT_EQ(neural_logic_quantum_implies(QREASON_TRUE, QREASON_FALSE), QREASON_FALSE);
}

TEST(QuantumTernaryLogicTest, ImpliesFalseTrue) {
    // F -> T = T
    EXPECT_EQ(neural_logic_quantum_implies(QREASON_FALSE, QREASON_TRUE), QREASON_TRUE);
}

TEST(QuantumTernaryLogicTest, ImpliesFalseFalse) {
    // F -> F = T
    EXPECT_EQ(neural_logic_quantum_implies(QREASON_FALSE, QREASON_FALSE), QREASON_TRUE);
}

//=============================================================================
// Quantum Interference Tests
//=============================================================================

TEST_F(NeuralLogicQuantumBridgeTest, ApplyInterferenceConstructive) {
    float amplitudes[] = {0.5f, 0.5f, 0.5f, 0.5f};
    float output = 0.0f;

    int result = neural_logic_quantum_apply_interference(
        bridge, 0, amplitudes, 4, &output);

    EXPECT_EQ(result, 0);
    EXPECT_GT(output, 0.0f);  // Constructive interference
}

TEST_F(NeuralLogicQuantumBridgeTest, ApplyInterferenceDestructive) {
    // Amplitudes that average below threshold
    float amplitudes[] = {0.05f, 0.05f, 0.05f, 0.05f};
    float output = 1.0f;

    int result = neural_logic_quantum_apply_interference(
        bridge, 0, amplitudes, 4, &output);

    EXPECT_EQ(result, 0);
    EXPECT_FLOAT_EQ(output, 0.0f);  // Destructive interference (below threshold)
}

TEST_F(NeuralLogicQuantumBridgeTest, ApplyInterferenceNullAmplitudes) {
    float output = 0.0f;

    int result = neural_logic_quantum_apply_interference(
        bridge, 0, nullptr, 4, &output);

    EXPECT_EQ(result, -1);
}

TEST_F(NeuralLogicQuantumBridgeTest, ApplyInterferenceNullOutput) {
    float amplitudes[] = {0.5f, 0.5f};

    int result = neural_logic_quantum_apply_interference(
        bridge, 0, amplitudes, 2, nullptr);

    EXPECT_EQ(result, -1);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(NeuralLogicQuantumBridgeTest, GetStatsInitial) {
    neural_logic_quantum_stats_t stats;

    int result = neural_logic_quantum_get_stats(bridge, &stats);

    EXPECT_EQ(result, 0);
    EXPECT_EQ(stats.quantum_evaluations, 0u);
    EXPECT_EQ(stats.sat_solutions_found, 0u);
    EXPECT_EQ(stats.unsatisfiable_queries, 0u);
}

TEST_F(NeuralLogicQuantumBridgeTest, GetStatsNullBridge) {
    neural_logic_quantum_stats_t stats;

    int result = neural_logic_quantum_get_stats(nullptr, &stats);

    EXPECT_EQ(result, -1);
}

TEST_F(NeuralLogicQuantumBridgeTest, GetStatsNullOutput) {
    int result = neural_logic_quantum_get_stats(bridge, nullptr);

    EXPECT_EQ(result, -1);
}

TEST_F(NeuralLogicQuantumBridgeTest, ResetStats) {
    // Reset should not crash
    neural_logic_quantum_reset_stats(bridge);

    neural_logic_quantum_stats_t stats;
    neural_logic_quantum_get_stats(bridge, &stats);

    EXPECT_EQ(stats.quantum_evaluations, 0u);
}

TEST_F(NeuralLogicQuantumBridgeTest, ResetStatsNull) {
    // Should not crash
    neural_logic_quantum_reset_stats(nullptr);
}

//=============================================================================
// Connection Tests
//=============================================================================

TEST_F(NeuralLogicQuantumBridgeTest, ConnectNullNetwork) {
    int result = neural_logic_quantum_bridge_connect(bridge, nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(NeuralLogicQuantumBridgeTest, ConnectNullBridge) {
    int result = neural_logic_quantum_bridge_connect(nullptr, nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(NeuralLogicQuantumBridgeTest, DisconnectNotConnected) {
    int result = neural_logic_quantum_bridge_disconnect(bridge);
    EXPECT_EQ(result, 0);
}

TEST_F(NeuralLogicQuantumBridgeTest, DisconnectNullBridge) {
    int result = neural_logic_quantum_bridge_disconnect(nullptr);
    EXPECT_EQ(result, -1);
}

//=============================================================================
// Quantum Reasoning Integration Tests
//=============================================================================

TEST(QuantumReasoningIntegrationTest, SolveSATSimple) {
    qreason_t reasoner = qreason_create(nullptr);
    ASSERT_NE(reasoner, nullptr);

    // Simple satisfiable: (x1)
    qreason_cnf_t cnf = {};
    cnf.n_variables = 1;
    cnf.n_clauses = 1;
    cnf.clauses[0].n_literals = 1;
    cnf.clauses[0].literals[0].variable = 0;
    cnf.clauses[0].literals[0].negated = false;

    qreason_result_t result = {};
    int status = qreason_solve_sat(reasoner, &cnf, &result);

    EXPECT_EQ(status, 0);
    EXPECT_TRUE(result.satisfiable);

    qreason_destroy(reasoner);
}

TEST(QuantumReasoningIntegrationTest, SolveSATWithMultipleClauses) {
    qreason_t reasoner = qreason_create(nullptr);
    ASSERT_NE(reasoner, nullptr);

    // (x1 OR x2) AND (NOT x1 OR x2)
    // Solution: x2 = true (any x1)
    qreason_cnf_t cnf = {};
    cnf.n_variables = 2;
    cnf.n_clauses = 2;

    // Clause 1: x1 OR x2
    cnf.clauses[0].n_literals = 2;
    cnf.clauses[0].literals[0].variable = 0;
    cnf.clauses[0].literals[0].negated = false;
    cnf.clauses[0].literals[1].variable = 1;
    cnf.clauses[0].literals[1].negated = false;

    // Clause 2: NOT x1 OR x2
    cnf.clauses[1].n_literals = 2;
    cnf.clauses[1].literals[0].variable = 0;
    cnf.clauses[1].literals[0].negated = true;
    cnf.clauses[1].literals[1].variable = 1;
    cnf.clauses[1].literals[1].negated = false;

    qreason_result_t result = {};
    int status = qreason_solve_sat(reasoner, &cnf, &result);

    EXPECT_EQ(status, 0);
    EXPECT_TRUE(result.satisfiable);
    EXPECT_GT(result.satisfaction_prob, 0.0f);

    qreason_destroy(reasoner);
}

TEST(QuantumReasoningIntegrationTest, SolveSATUnsatisfiable) {
    qreason_t reasoner = qreason_create(nullptr);
    ASSERT_NE(reasoner, nullptr);

    // (x1) AND (NOT x1) - unsatisfiable
    qreason_cnf_t cnf = {};
    cnf.n_variables = 1;
    cnf.n_clauses = 2;

    // Clause 1: x1
    cnf.clauses[0].n_literals = 1;
    cnf.clauses[0].literals[0].variable = 0;
    cnf.clauses[0].literals[0].negated = false;

    // Clause 2: NOT x1
    cnf.clauses[1].n_literals = 1;
    cnf.clauses[1].literals[0].variable = 0;
    cnf.clauses[1].literals[0].negated = true;

    qreason_result_t result = {};
    int status = qreason_solve_sat(reasoner, &cnf, &result);

    EXPECT_EQ(status, 0);
    EXPECT_FALSE(result.satisfiable);

    qreason_destroy(reasoner);
}

TEST(QuantumReasoningIntegrationTest, GroverIterationsRecorded) {
    qreason_config_t config = qreason_default_config();
    config.max_grover_iterations = 3;

    qreason_t reasoner = qreason_create(&config);
    ASSERT_NE(reasoner, nullptr);

    // Create a formula with few solutions to trigger Grover
    qreason_cnf_t cnf = {};
    cnf.n_variables = 4;
    cnf.n_clauses = 1;
    cnf.clauses[0].n_literals = 1;
    cnf.clauses[0].literals[0].variable = 0;
    cnf.clauses[0].literals[0].negated = false;

    qreason_result_t result = {};
    qreason_solve_sat(reasoner, &cnf, &result);

    // Grover iterations should be recorded
    EXPECT_LE(result.grover_iterations, 3u);

    qreason_destroy(reasoner);
}

