/**
 * @file test_brain_quantum_reasoning.cpp
 * @brief Unit Tests for Brain Quantum Reasoning Integration
 * @version 1.0.0
 * @date 2025-12-30
 *
 * TEST COVERAGE: 25+ tests covering:
 * - Lifecycle (create/destroy)
 * - SAT solving
 * - Knowledge base operations
 * - Ternary logic queries
 * - Modulation (fatigue/stress)
 * - Statistics tracking
 */

#include <gtest/gtest.h>
#include <cmath>

// Headers have their own extern "C" guards
#include "core/brain/inference/nimcp_brain_quantum_reasoning.h"
#include "core/brain/nimcp_brain.h"

//=============================================================================
// Test Fixture
//=============================================================================

class BrainQuantumReasoningTest : public ::testing::Test {
protected:
    static brain_t shared_brain;

    // Shared brain created once for all tests in this suite
    static void SetUpTestSuite() {
        shared_brain = brain_create("quantum_test", BRAIN_SIZE_MICRO,
                            BRAIN_TASK_CLASSIFICATION, 4, 2);
        ASSERT_NE(shared_brain, nullptr);

        // Initialize quantum reasoning
        bool result = nimcp_brain_factory_init_quantum_reasoning(shared_brain, nullptr);
        ASSERT_TRUE(result);
    }

    static void TearDownTestSuite() {
        if (shared_brain) {
            nimcp_brain_qreason_destroy(shared_brain);
            brain_destroy(shared_brain);
            shared_brain = nullptr;
        }
    }

    // Per-test accessor
    brain_t brain = nullptr;

    void SetUp() override {
        brain = shared_brain;
        ASSERT_NE(brain, nullptr);
        // Reset state for each test: re-enable and clear KB
        nimcp_brain_qreason_set_enabled(brain, true);
        nimcp_brain_qreason_clear_kb(brain);
        nimcp_brain_qreason_reset_stats(brain);
        nimcp_brain_qreason_set_fatigue(brain, 0.0f);
        nimcp_brain_qreason_set_stress(brain, 0.0f);
    }
};

brain_t BrainQuantumReasoningTest::shared_brain = nullptr;

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST(BrainQuantumReasoningLifecycleTest, DefaultConfig) {
    brain_qreason_config_t config = brain_qreason_default_config();

    EXPECT_TRUE(config.enabled);
    EXPECT_EQ(config.max_grover_iterations, 0u);  // Auto
    EXPECT_EQ(config.max_inference_depth, 20u);
    EXPECT_FLOAT_EQ(config.min_confidence, 0.5f);
    EXPECT_TRUE(config.use_ternary_logic);
    EXPECT_TRUE(config.enable_interference);
}

TEST(BrainQuantumReasoningLifecycleTest, InitWithDefaults) {
    brain_t brain = brain_create("lifecycle_test", BRAIN_SIZE_MICRO,
                                 BRAIN_TASK_CLASSIFICATION, 4, 2);
    ASSERT_NE(brain, nullptr);

    bool result = nimcp_brain_factory_init_quantum_reasoning(brain, nullptr);
    EXPECT_TRUE(result);

    EXPECT_TRUE(nimcp_brain_qreason_is_enabled(brain));

    nimcp_brain_qreason_destroy(brain);
    brain_destroy(brain);
}

TEST(BrainQuantumReasoningLifecycleTest, InitWithCustomConfig) {
    brain_t brain = brain_create("custom_config_test", BRAIN_SIZE_MICRO,
                                 BRAIN_TASK_CLASSIFICATION, 4, 2);
    ASSERT_NE(brain, nullptr);

    brain_qreason_config_t qconfig = brain_qreason_default_config();
    qconfig.max_grover_iterations = 5;
    qconfig.min_confidence = 0.7f;

    bool result = nimcp_brain_factory_init_quantum_reasoning(brain, &qconfig);
    EXPECT_TRUE(result);

    nimcp_brain_qreason_destroy(brain);
    brain_destroy(brain);
}

TEST(BrainQuantumReasoningLifecycleTest, InitNullBrain) {
    bool result = nimcp_brain_factory_init_quantum_reasoning(nullptr, nullptr);
    EXPECT_FALSE(result);
}

TEST(BrainQuantumReasoningLifecycleTest, DestroyNull) {
    // Should not crash
    nimcp_brain_qreason_destroy(nullptr);
}

TEST(BrainQuantumReasoningLifecycleTest, IsEnabledNull) {
    EXPECT_FALSE(nimcp_brain_qreason_is_enabled(nullptr));
}

//=============================================================================
// Enable/Disable Tests
//=============================================================================

TEST_F(BrainQuantumReasoningTest, IsEnabled) {
    EXPECT_TRUE(nimcp_brain_qreason_is_enabled(brain));
}

TEST_F(BrainQuantumReasoningTest, SetDisabled) {
    int result = nimcp_brain_qreason_set_enabled(brain, false);
    EXPECT_EQ(result, 0);
    EXPECT_FALSE(nimcp_brain_qreason_is_enabled(brain));
}

TEST_F(BrainQuantumReasoningTest, SetEnabledNull) {
    int result = nimcp_brain_qreason_set_enabled(nullptr, true);
    EXPECT_EQ(result, -1);
}

//=============================================================================
// SAT Solving Tests
//=============================================================================

TEST_F(BrainQuantumReasoningTest, SolveSATSimple) {
    brain_reasoning_query_t query = {};
    snprintf(query.description, sizeof(query.description), "Simple SAT");

    // Simple satisfiable: (x0)
    query.cnf.n_variables = 1;
    query.cnf.n_clauses = 1;
    query.cnf.clauses[0].n_literals = 1;
    query.cnf.clauses[0].literals[0].variable = 0;
    query.cnf.clauses[0].literals[0].negated = false;

    brain_reasoning_result_t result = {};
    int status = nimcp_brain_qreason_solve_sat(brain, &query, &result);

    EXPECT_EQ(status, 0);
    EXPECT_TRUE(result.satisfiable);
    EXPECT_GT(result.satisfaction_probability, 0.0f);
}

TEST_F(BrainQuantumReasoningTest, SolveSATUnsatisfiable) {
    brain_reasoning_query_t query = {};
    snprintf(query.description, sizeof(query.description), "UNSAT test");

    // Unsatisfiable: (x0) AND (NOT x0)
    query.cnf.n_variables = 1;
    query.cnf.n_clauses = 2;

    // Clause 0: x0
    query.cnf.clauses[0].n_literals = 1;
    query.cnf.clauses[0].literals[0].variable = 0;
    query.cnf.clauses[0].literals[0].negated = false;

    // Clause 1: NOT x0
    query.cnf.clauses[1].n_literals = 1;
    query.cnf.clauses[1].literals[0].variable = 0;
    query.cnf.clauses[1].literals[0].negated = true;

    brain_reasoning_result_t result = {};
    int status = nimcp_brain_qreason_solve_sat(brain, &query, &result);

    EXPECT_EQ(status, 0);
    EXPECT_FALSE(result.satisfiable);
}

TEST_F(BrainQuantumReasoningTest, SolveSATMultipleClauses) {
    brain_reasoning_query_t query = {};
    snprintf(query.description, sizeof(query.description), "Multi-clause SAT");

    // (x0 OR x1) AND (NOT x0 OR x1) - forces x1 = true
    query.cnf.n_variables = 2;
    query.cnf.n_clauses = 2;

    // Clause 0: x0 OR x1
    query.cnf.clauses[0].n_literals = 2;
    query.cnf.clauses[0].literals[0].variable = 0;
    query.cnf.clauses[0].literals[0].negated = false;
    query.cnf.clauses[0].literals[1].variable = 1;
    query.cnf.clauses[0].literals[1].negated = false;

    // Clause 1: NOT x0 OR x1
    query.cnf.clauses[1].n_literals = 2;
    query.cnf.clauses[1].literals[0].variable = 0;
    query.cnf.clauses[1].literals[0].negated = true;
    query.cnf.clauses[1].literals[1].variable = 1;
    query.cnf.clauses[1].literals[1].negated = false;

    brain_reasoning_result_t result = {};
    int status = nimcp_brain_qreason_solve_sat(brain, &query, &result);

    EXPECT_EQ(status, 0);
    EXPECT_TRUE(result.satisfiable);
}

TEST_F(BrainQuantumReasoningTest, SolveSATNullArgs) {
    brain_reasoning_query_t query = {};
    brain_reasoning_result_t result = {};

    EXPECT_EQ(nimcp_brain_qreason_solve_sat(nullptr, &query, &result), -1);
    EXPECT_EQ(nimcp_brain_qreason_solve_sat(brain, nullptr, &result), -1);
    EXPECT_EQ(nimcp_brain_qreason_solve_sat(brain, &query, nullptr), -1);
}

//=============================================================================
// Knowledge Base Tests
//=============================================================================

TEST_F(BrainQuantumReasoningTest, SetAndGetFact) {
    int status = nimcp_brain_qreason_set_fact(brain, 0, QREASON_TRUE, 0.9f);
    EXPECT_EQ(status, 0);

    qreason_truth_t value;
    float confidence;
    status = nimcp_brain_qreason_get_fact(brain, 0, &value, &confidence);

    EXPECT_EQ(status, 0);
    EXPECT_EQ(value, QREASON_TRUE);
    EXPECT_FLOAT_EQ(confidence, 0.9f);
}

TEST_F(BrainQuantumReasoningTest, SetFactFalse) {
    int status = nimcp_brain_qreason_set_fact(brain, 1, QREASON_FALSE, 0.8f);
    EXPECT_EQ(status, 0);

    qreason_truth_t value;
    float confidence;
    status = nimcp_brain_qreason_get_fact(brain, 1, &value, &confidence);

    EXPECT_EQ(status, 0);
    EXPECT_EQ(value, QREASON_FALSE);
}

TEST_F(BrainQuantumReasoningTest, SetFactUnknown) {
    int status = nimcp_brain_qreason_set_fact(brain, 2, QREASON_UNKNOWN, 0.0f);
    EXPECT_EQ(status, 0);

    qreason_truth_t value;
    float confidence;
    status = nimcp_brain_qreason_get_fact(brain, 2, &value, &confidence);

    EXPECT_EQ(status, 0);
    EXPECT_EQ(value, QREASON_UNKNOWN);
}

TEST_F(BrainQuantumReasoningTest, ClearKB) {
    nimcp_brain_qreason_set_fact(brain, 0, QREASON_TRUE, 0.9f);
    nimcp_brain_qreason_set_fact(brain, 1, QREASON_FALSE, 0.8f);

    int status = nimcp_brain_qreason_clear_kb(brain);
    EXPECT_EQ(status, 0);

    qreason_truth_t value;
    float confidence;
    nimcp_brain_qreason_get_fact(brain, 0, &value, &confidence);
    EXPECT_EQ(value, QREASON_UNKNOWN);
}

//=============================================================================
// Ternary Logic Tests
//=============================================================================

TEST_F(BrainQuantumReasoningTest, QueryTernaryTrue) {
    nimcp_brain_qreason_set_fact(brain, 5, QREASON_TRUE, 0.95f);

    qreason_truth_t value;
    float confidence;
    int status = nimcp_brain_qreason_query_ternary(brain, 5, &value, &confidence);

    EXPECT_EQ(status, 0);
    EXPECT_EQ(value, QREASON_TRUE);
    EXPECT_FLOAT_EQ(confidence, 0.95f);
}

TEST_F(BrainQuantumReasoningTest, QueryTernaryFalse) {
    nimcp_brain_qreason_set_fact(brain, 6, QREASON_FALSE, 0.85f);

    qreason_truth_t value;
    float confidence;
    int status = nimcp_brain_qreason_query_ternary(brain, 6, &value, &confidence);

    EXPECT_EQ(status, 0);
    EXPECT_EQ(value, QREASON_FALSE);
}

TEST_F(BrainQuantumReasoningTest, QueryTernaryUnknown) {
    qreason_truth_t value;
    float confidence;
    int status = nimcp_brain_qreason_query_ternary(brain, 10, &value, &confidence);

    EXPECT_EQ(status, 0);
    EXPECT_EQ(value, QREASON_UNKNOWN);
}

//=============================================================================
// Modulation Tests
//=============================================================================

TEST_F(BrainQuantumReasoningTest, SetFatigue) {
    int status = nimcp_brain_qreason_set_fatigue(brain, 0.5f);
    EXPECT_EQ(status, 0);
}

TEST_F(BrainQuantumReasoningTest, SetFatigueClamp) {
    EXPECT_EQ(nimcp_brain_qreason_set_fatigue(brain, -0.5f), 0);
    EXPECT_EQ(nimcp_brain_qreason_set_fatigue(brain, 1.5f), 0);
}

TEST_F(BrainQuantumReasoningTest, SetFatigueNull) {
    int status = nimcp_brain_qreason_set_fatigue(nullptr, 0.5f);
    EXPECT_EQ(status, -1);
}

TEST_F(BrainQuantumReasoningTest, SetStress) {
    int status = nimcp_brain_qreason_set_stress(brain, 0.7f);
    EXPECT_EQ(status, 0);
}

TEST_F(BrainQuantumReasoningTest, SetStressNull) {
    int status = nimcp_brain_qreason_set_stress(nullptr, 0.5f);
    EXPECT_EQ(status, -1);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(BrainQuantumReasoningTest, GetStatsInitial) {
    brain_qreason_stats_t stats;
    int status = nimcp_brain_qreason_get_stats(brain, &stats);

    EXPECT_EQ(status, 0);
    EXPECT_EQ(stats.total_queries, 0u);
    EXPECT_EQ(stats.satisfiable_count, 0u);
    EXPECT_EQ(stats.unsatisfiable_count, 0u);
}

TEST_F(BrainQuantumReasoningTest, GetStatsAfterQuery) {
    // Run a query
    brain_reasoning_query_t query = {};
    query.cnf.n_variables = 1;
    query.cnf.n_clauses = 1;
    query.cnf.clauses[0].n_literals = 1;
    query.cnf.clauses[0].literals[0].variable = 0;
    query.cnf.clauses[0].literals[0].negated = false;

    brain_reasoning_result_t result = {};
    nimcp_brain_qreason_solve_sat(brain, &query, &result);

    // Check stats
    brain_qreason_stats_t stats;
    nimcp_brain_qreason_get_stats(brain, &stats);

    EXPECT_EQ(stats.total_queries, 1u);
    EXPECT_GE(stats.satisfiable_count + stats.unsatisfiable_count, 1u);
}

TEST_F(BrainQuantumReasoningTest, ResetStats) {
    // Run a query
    brain_reasoning_query_t query = {};
    query.cnf.n_variables = 1;
    query.cnf.n_clauses = 1;
    query.cnf.clauses[0].n_literals = 1;
    query.cnf.clauses[0].literals[0].variable = 0;
    query.cnf.clauses[0].literals[0].negated = false;

    brain_reasoning_result_t result = {};
    nimcp_brain_qreason_solve_sat(brain, &query, &result);

    // Reset
    nimcp_brain_qreason_reset_stats(brain);

    // Verify reset
    brain_qreason_stats_t stats;
    nimcp_brain_qreason_get_stats(brain, &stats);

    EXPECT_EQ(stats.total_queries, 0u);
}

TEST_F(BrainQuantumReasoningTest, GetStatsNullArgs) {
    brain_qreason_stats_t stats;
    EXPECT_EQ(nimcp_brain_qreason_get_stats(nullptr, &stats), -1);
    EXPECT_EQ(nimcp_brain_qreason_get_stats(brain, nullptr), -1);
}

//=============================================================================
// Handle Access Tests
//=============================================================================

TEST_F(BrainQuantumReasoningTest, GetHandle) {
    qreason_t handle = nimcp_brain_qreason_get_handle(brain);
    EXPECT_NE(handle, nullptr);
}

TEST_F(BrainQuantumReasoningTest, GetHandleNull) {
    qreason_t handle = nimcp_brain_qreason_get_handle(nullptr);
    EXPECT_EQ(handle, nullptr);
}
