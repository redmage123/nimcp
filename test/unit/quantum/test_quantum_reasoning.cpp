/**
 * @file test_quantum_reasoning.cpp
 * @brief Comprehensive unit tests for quantum reasoning module
 *
 * Tests cover:
 * - Quantum reasoner lifecycle (create, destroy)
 * - Knowledge base management (facts, rules)
 * - Ternary logic operations (AND, OR, NOT, IMPLIES)
 * - Quantum state operations (init, oracle, diffusion, measure)
 * - Forward chaining inference
 * - SAT solving with Grover-inspired search
 * - Configuration and statistics
 *
 * @version Phase C2: Quantum Reasoning Integration
 * @date 2025-12-30
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

// Headers have their own extern "C" guards
#include "cognitive/reasoning/nimcp_quantum_reasoning.h"

//=============================================================================
// Test Fixture
//=============================================================================

class QuantumReasoningTest : public ::testing::Test {
protected:
    qreason_t reasoner = nullptr;

    void SetUp() override {
        qreason_config_t config = qreason_default_config();
        reasoner = qreason_create(&config);
    }

    void TearDown() override {
        if (reasoner) {
            qreason_destroy(reasoner);
            reasoner = nullptr;
        }
    }
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(QuantumReasoningTest, CreateWithNullConfig) {
    qreason_t r = qreason_create(nullptr);
    ASSERT_NE(r, nullptr);
    qreason_destroy(r);
}

TEST_F(QuantumReasoningTest, CreateWithConfig) {
    ASSERT_NE(reasoner, nullptr);
}

TEST_F(QuantumReasoningTest, DestroyNull) {
    qreason_destroy(nullptr);  /* Should not crash */
}

TEST_F(QuantumReasoningTest, DefaultConfigValues) {
    qreason_config_t config = qreason_default_config();
    EXPECT_EQ(config.max_grover_iterations, 10u);
    EXPECT_EQ(config.max_inference_depth, 20u);
    EXPECT_FLOAT_EQ(config.min_confidence, 0.5f);
    EXPECT_TRUE(config.use_ternary_logic);
    EXPECT_TRUE(config.enable_interference);
    EXPECT_EQ(config.seed, QREASON_DEFAULT_SEED);
}

TEST_F(QuantumReasoningTest, GetConfig) {
    qreason_config_t config;
    int result = qreason_get_config(reasoner, &config);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(config.max_grover_iterations, 10u);
}

TEST_F(QuantumReasoningTest, GetConfigNullReasoner) {
    qreason_config_t config;
    int result = qreason_get_config(nullptr, &config);
    EXPECT_EQ(result, -1);
}

TEST_F(QuantumReasoningTest, GetConfigNullOutput) {
    int result = qreason_get_config(reasoner, nullptr);
    EXPECT_EQ(result, -1);
}

//=============================================================================
// Fact Management Tests
//=============================================================================

TEST_F(QuantumReasoningTest, SetFact) {
    int result = qreason_set_fact(reasoner, 0, QREASON_TRUE, 0.9f);
    EXPECT_EQ(result, 0);
}

TEST_F(QuantumReasoningTest, SetFactInvalidVariable) {
    int result = qreason_set_fact(reasoner, QREASON_MAX_VARIABLES, QREASON_TRUE, 0.9f);
    EXPECT_EQ(result, -1);
}

TEST_F(QuantumReasoningTest, SetFactNullReasoner) {
    int result = qreason_set_fact(nullptr, 0, QREASON_TRUE, 0.9f);
    EXPECT_EQ(result, -1);
}

TEST_F(QuantumReasoningTest, GetFact) {
    qreason_set_fact(reasoner, 5, QREASON_TRUE, 0.85f);

    float confidence = 0.0f;
    qreason_truth_t value = qreason_get_fact(reasoner, 5, &confidence);

    EXPECT_EQ(value, QREASON_TRUE);
    EXPECT_FLOAT_EQ(confidence, 0.85f);
}

TEST_F(QuantumReasoningTest, GetFactUnset) {
    float confidence = 1.0f;
    qreason_truth_t value = qreason_get_fact(reasoner, 10, &confidence);

    EXPECT_EQ(value, QREASON_UNKNOWN);
    EXPECT_FLOAT_EQ(confidence, 0.0f);
}

TEST_F(QuantumReasoningTest, GetFactNullConfidence) {
    qreason_set_fact(reasoner, 3, QREASON_FALSE, 0.7f);
    qreason_truth_t value = qreason_get_fact(reasoner, 3, nullptr);
    EXPECT_EQ(value, QREASON_FALSE);
}

TEST_F(QuantumReasoningTest, ClearFacts) {
    qreason_set_fact(reasoner, 0, QREASON_TRUE, 0.9f);
    qreason_set_fact(reasoner, 1, QREASON_FALSE, 0.8f);

    qreason_clear_facts(reasoner);

    float conf;
    EXPECT_EQ(qreason_get_fact(reasoner, 0, &conf), QREASON_UNKNOWN);
    EXPECT_EQ(qreason_get_fact(reasoner, 1, &conf), QREASON_UNKNOWN);
}

TEST_F(QuantumReasoningTest, ClearFactsNull) {
    qreason_clear_facts(nullptr);  /* Should not crash */
}

//=============================================================================
// Rule Management Tests
//=============================================================================

TEST_F(QuantumReasoningTest, AddRule) {
    uint32_t antecedents[] = {0, 1};
    int result = qreason_add_rule(reasoner, antecedents, 2, 2, 0.95f);
    EXPECT_EQ(result, 0);
}

TEST_F(QuantumReasoningTest, AddRuleTooManyAntecedents) {
    uint32_t antecedents[QREASON_MAX_LITERALS + 1];
    int result = qreason_add_rule(reasoner, antecedents, QREASON_MAX_LITERALS + 1, 0, 0.9f);
    EXPECT_EQ(result, -1);
}

TEST_F(QuantumReasoningTest, AddRuleNullReasoner) {
    uint32_t antecedents[] = {0};
    int result = qreason_add_rule(nullptr, antecedents, 1, 1, 0.9f);
    EXPECT_EQ(result, -1);
}

TEST_F(QuantumReasoningTest, ClearRules) {
    uint32_t antecedents[] = {0};
    qreason_add_rule(reasoner, antecedents, 1, 1, 0.9f);
    qreason_clear_rules(reasoner);

    /* Rules should be cleared - test via internal state */
    qreason_internal_t* internal = (qreason_internal_t*)reasoner;
    EXPECT_EQ(internal->kb.n_rules, 0u);
}

TEST_F(QuantumReasoningTest, ClearRulesNull) {
    qreason_clear_rules(nullptr);  /* Should not crash */
}

//=============================================================================
// Ternary Logic Tests
//=============================================================================

TEST_F(QuantumReasoningTest, TernaryAndTrueTrue) {
    EXPECT_EQ(qreason_and(QREASON_TRUE, QREASON_TRUE), QREASON_TRUE);
}

TEST_F(QuantumReasoningTest, TernaryAndTrueFalse) {
    EXPECT_EQ(qreason_and(QREASON_TRUE, QREASON_FALSE), QREASON_FALSE);
}

TEST_F(QuantumReasoningTest, TernaryAndFalseUnknown) {
    EXPECT_EQ(qreason_and(QREASON_FALSE, QREASON_UNKNOWN), QREASON_FALSE);
}

TEST_F(QuantumReasoningTest, TernaryAndTrueUnknown) {
    EXPECT_EQ(qreason_and(QREASON_TRUE, QREASON_UNKNOWN), QREASON_UNKNOWN);
}

TEST_F(QuantumReasoningTest, TernaryAndUnknownUnknown) {
    EXPECT_EQ(qreason_and(QREASON_UNKNOWN, QREASON_UNKNOWN), QREASON_UNKNOWN);
}

TEST_F(QuantumReasoningTest, TernaryOrTrueFalse) {
    EXPECT_EQ(qreason_or(QREASON_TRUE, QREASON_FALSE), QREASON_TRUE);
}

TEST_F(QuantumReasoningTest, TernaryOrFalseFalse) {
    EXPECT_EQ(qreason_or(QREASON_FALSE, QREASON_FALSE), QREASON_FALSE);
}

TEST_F(QuantumReasoningTest, TernaryOrFalseUnknown) {
    EXPECT_EQ(qreason_or(QREASON_FALSE, QREASON_UNKNOWN), QREASON_UNKNOWN);
}

TEST_F(QuantumReasoningTest, TernaryOrTrueUnknown) {
    EXPECT_EQ(qreason_or(QREASON_TRUE, QREASON_UNKNOWN), QREASON_TRUE);
}

TEST_F(QuantumReasoningTest, TernaryNotTrue) {
    EXPECT_EQ(qreason_not(QREASON_TRUE), QREASON_FALSE);
}

TEST_F(QuantumReasoningTest, TernaryNotFalse) {
    EXPECT_EQ(qreason_not(QREASON_FALSE), QREASON_TRUE);
}

TEST_F(QuantumReasoningTest, TernaryNotUnknown) {
    EXPECT_EQ(qreason_not(QREASON_UNKNOWN), QREASON_UNKNOWN);
}

TEST_F(QuantumReasoningTest, TernaryImpliesTrueTrue) {
    /* TRUE -> TRUE = TRUE */
    EXPECT_EQ(qreason_implies(QREASON_TRUE, QREASON_TRUE), QREASON_TRUE);
}

TEST_F(QuantumReasoningTest, TernaryImpliesTrueFalse) {
    /* TRUE -> FALSE = FALSE */
    EXPECT_EQ(qreason_implies(QREASON_TRUE, QREASON_FALSE), QREASON_FALSE);
}

TEST_F(QuantumReasoningTest, TernaryImpliesFalseAnything) {
    /* FALSE -> X = TRUE (vacuously true) */
    EXPECT_EQ(qreason_implies(QREASON_FALSE, QREASON_TRUE), QREASON_TRUE);
    EXPECT_EQ(qreason_implies(QREASON_FALSE, QREASON_FALSE), QREASON_TRUE);
    EXPECT_EQ(qreason_implies(QREASON_FALSE, QREASON_UNKNOWN), QREASON_TRUE);
}

TEST_F(QuantumReasoningTest, TernaryImpliesUnknown) {
    /* UNKNOWN -> X gives UNKNOWN or TRUE depending on X */
    EXPECT_EQ(qreason_implies(QREASON_UNKNOWN, QREASON_TRUE), QREASON_TRUE);
    EXPECT_EQ(qreason_implies(QREASON_UNKNOWN, QREASON_UNKNOWN), QREASON_UNKNOWN);
}

//=============================================================================
// Quantum State Tests
//=============================================================================

TEST_F(QuantumReasoningTest, QStateInit) {
    qreason_qstate_t qstate;
    qreason_qstate_init(&qstate, 3);

    EXPECT_EQ(qstate.n_qubits, 3u);
    EXPECT_EQ(qstate.state_dim, 8u);  /* 2^3 = 8 */

    /* Check uniform superposition */
    float expected = 1.0f / sqrtf(8.0f);
    for (uint32_t i = 0; i < 8; i++) {
        EXPECT_NEAR(qstate.amplitudes[i], expected, 1e-6f);
    }
}

TEST_F(QuantumReasoningTest, QStateInitCapped) {
    qreason_qstate_t qstate;
    qreason_qstate_init(&qstate, 20);  /* Should cap at 16 */

    EXPECT_EQ(qstate.n_qubits, 16u);
    EXPECT_EQ(qstate.state_dim, 65536u);  /* 2^16 */
}

TEST_F(QuantumReasoningTest, Diffusion) {
    qreason_qstate_t qstate;
    qreason_qstate_init(&qstate, 3);

    /* Apply phase flip to state 0 */
    qstate.amplitudes[0] *= -1.0f;

    /* Apply diffusion */
    qreason_diffusion(&qstate);

    /* State 0 should have higher amplitude after diffusion (amplitude amplification) */
    EXPECT_GT(fabsf(qstate.amplitudes[0]), 1.0f / sqrtf(8.0f));
}

TEST_F(QuantumReasoningTest, Measure) {
    qreason_qstate_t qstate;
    qreason_qstate_init(&qstate, 2);  /* 4 states */

    /* Set state 3 to have highest amplitude */
    qstate.amplitudes[0] = 0.1f;
    qstate.amplitudes[1] = 0.1f;
    qstate.amplitudes[2] = 0.1f;
    qstate.amplitudes[3] = 0.9f;  /* Highest */

    uint32_t result = qreason_measure(&qstate);
    EXPECT_EQ(result, 3u);
}

//=============================================================================
// CNF Clause Tests
//=============================================================================

TEST_F(QuantumReasoningTest, MakeClause) {
    int literals[] = {1, -2, 3};  /* x0 OR NOT x1 OR x2 */
    qreason_clause_t clause = qreason_make_clause(literals, 3);

    EXPECT_EQ(clause.n_literals, 3u);

    /* Literal 1 (x0) */
    EXPECT_EQ(clause.literals[0].variable, 0u);
    EXPECT_FALSE(clause.literals[0].negated);

    /* Literal -2 (NOT x1) */
    EXPECT_EQ(clause.literals[1].variable, 1u);
    EXPECT_TRUE(clause.literals[1].negated);

    /* Literal 3 (x2) */
    EXPECT_EQ(clause.literals[2].variable, 2u);
    EXPECT_FALSE(clause.literals[2].negated);
}

TEST_F(QuantumReasoningTest, MakeClauseCapped) {
    int literals[QREASON_MAX_LITERALS + 5];
    for (int i = 0; i < QREASON_MAX_LITERALS + 5; i++) {
        literals[i] = i + 1;
    }

    qreason_clause_t clause = qreason_make_clause(literals, QREASON_MAX_LITERALS + 5);
    EXPECT_EQ(clause.n_literals, QREASON_MAX_LITERALS);
}

//=============================================================================
// Oracle Tests
//=============================================================================

TEST_F(QuantumReasoningTest, OracleCNFSimple) {
    /* Create simple CNF: x0 AND x1 */
    qreason_cnf_t cnf = {0};
    cnf.n_variables = 2;
    cnf.n_clauses = 2;

    /* Clause 0: x0 */
    cnf.clauses[0].n_literals = 1;
    cnf.clauses[0].literals[0].variable = 0;
    cnf.clauses[0].literals[0].negated = false;

    /* Clause 1: x1 */
    cnf.clauses[1].n_literals = 1;
    cnf.clauses[1].literals[0].variable = 1;
    cnf.clauses[1].literals[0].negated = false;

    qreason_qstate_t qstate;
    qreason_qstate_init(&qstate, 2);

    /* Apply oracle - should flip phase of state 3 (x0=1, x1=1) */
    qreason_oracle_cnf(&qstate, &cnf);

    /* State 3 should be negative (phase flipped) */
    EXPECT_LT(qstate.amplitudes[3], 0.0f);

    /* Other states should be positive (no flip) */
    EXPECT_GT(qstate.amplitudes[0], 0.0f);
    EXPECT_GT(qstate.amplitudes[1], 0.0f);
    EXPECT_GT(qstate.amplitudes[2], 0.0f);
}

TEST_F(QuantumReasoningTest, SatisfactionProbability) {
    /* Create CNF: x0 OR x1 (satisfied by 3 out of 4 states) */
    qreason_cnf_t cnf = {0};
    cnf.n_variables = 2;
    cnf.n_clauses = 1;

    cnf.clauses[0].n_literals = 2;
    cnf.clauses[0].literals[0].variable = 0;
    cnf.clauses[0].literals[0].negated = false;
    cnf.clauses[0].literals[1].variable = 1;
    cnf.clauses[0].literals[1].negated = false;

    qreason_qstate_t qstate;
    qreason_qstate_init(&qstate, 2);

    float prob = qreason_satisfaction_probability(&qstate, &cnf);
    EXPECT_NEAR(prob, 0.75f, 1e-4f);  /* 3/4 = 0.75 */
}

//=============================================================================
// SAT Solving Tests
//=============================================================================

TEST_F(QuantumReasoningTest, SolveSATEmpty) {
    qreason_cnf_t cnf = {0};
    cnf.n_variables = 0;
    cnf.n_clauses = 0;

    qreason_result_t result;
    int ret = qreason_solve_sat(reasoner, &cnf, &result);

    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(result.satisfiable);
    EXPECT_FLOAT_EQ(result.satisfaction_prob, 1.0f);
}

TEST_F(QuantumReasoningTest, SolveSATSimple) {
    /* x0 AND x1 */
    qreason_cnf_t cnf = {0};
    cnf.n_variables = 2;
    cnf.n_clauses = 2;

    cnf.clauses[0].n_literals = 1;
    cnf.clauses[0].literals[0].variable = 0;
    cnf.clauses[0].literals[0].negated = false;

    cnf.clauses[1].n_literals = 1;
    cnf.clauses[1].literals[0].variable = 1;
    cnf.clauses[1].literals[0].negated = false;

    qreason_result_t result;
    int ret = qreason_solve_sat(reasoner, &cnf, &result);

    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(result.satisfiable);
    EXPECT_EQ(result.assignment[0], QREASON_TRUE);
    EXPECT_EQ(result.assignment[1], QREASON_TRUE);
}

TEST_F(QuantumReasoningTest, SolveSATContradiction) {
    /* x0 AND NOT x0 - unsatisfiable */
    qreason_cnf_t cnf = {0};
    cnf.n_variables = 1;
    cnf.n_clauses = 2;

    cnf.clauses[0].n_literals = 1;
    cnf.clauses[0].literals[0].variable = 0;
    cnf.clauses[0].literals[0].negated = false;

    cnf.clauses[1].n_literals = 1;
    cnf.clauses[1].literals[0].variable = 0;
    cnf.clauses[1].literals[0].negated = true;

    qreason_result_t result;
    int ret = qreason_solve_sat(reasoner, &cnf, &result);

    EXPECT_EQ(ret, 0);
    EXPECT_FALSE(result.satisfiable);
    EXPECT_FLOAT_EQ(result.satisfaction_prob, 0.0f);
}

TEST_F(QuantumReasoningTest, SolveSATNull) {
    qreason_result_t result;
    EXPECT_EQ(qreason_solve_sat(nullptr, nullptr, &result), -1);
    EXPECT_EQ(qreason_solve_sat(reasoner, nullptr, &result), -1);

    qreason_cnf_t cnf = {0};
    EXPECT_EQ(qreason_solve_sat(reasoner, &cnf, nullptr), -1);
}

TEST_F(QuantumReasoningTest, SolveSATWithGrover) {
    /* 3-variable formula: (x0 OR x1) AND (x1 OR x2) AND (NOT x0 OR x2) */
    qreason_cnf_t cnf = {0};
    cnf.n_variables = 3;
    cnf.n_clauses = 3;

    /* Clause 0: x0 OR x1 */
    cnf.clauses[0].n_literals = 2;
    cnf.clauses[0].literals[0].variable = 0;
    cnf.clauses[0].literals[0].negated = false;
    cnf.clauses[0].literals[1].variable = 1;
    cnf.clauses[0].literals[1].negated = false;

    /* Clause 1: x1 OR x2 */
    cnf.clauses[1].n_literals = 2;
    cnf.clauses[1].literals[0].variable = 1;
    cnf.clauses[1].literals[0].negated = false;
    cnf.clauses[1].literals[1].variable = 2;
    cnf.clauses[1].literals[1].negated = false;

    /* Clause 2: NOT x0 OR x2 */
    cnf.clauses[2].n_literals = 2;
    cnf.clauses[2].literals[0].variable = 0;
    cnf.clauses[2].literals[0].negated = true;
    cnf.clauses[2].literals[1].variable = 2;
    cnf.clauses[2].literals[1].negated = false;

    qreason_result_t result;
    int ret = qreason_solve_sat(reasoner, &cnf, &result);

    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(result.satisfiable);
    EXPECT_GT(result.satisfaction_prob, 0.0f);
    EXPECT_GT(result.grover_iterations, 0u);
}

//=============================================================================
// Forward Chaining Tests
//=============================================================================

TEST_F(QuantumReasoningTest, ForwardChainSimple) {
    /* If A and B then C */
    /* Set A and B to TRUE */
    qreason_set_fact(reasoner, 0, QREASON_TRUE, 0.9f);  /* A */
    qreason_set_fact(reasoner, 1, QREASON_TRUE, 0.8f);  /* B */

    uint32_t antecedents[] = {0, 1};
    qreason_add_rule(reasoner, antecedents, 2, 2, 0.95f);  /* A AND B -> C */

    qreason_result_t result;
    uint32_t new_facts = qreason_forward_chain(reasoner, &result);

    EXPECT_EQ(new_facts, 1u);
    EXPECT_EQ(result.inferences_made, 1u);

    /* C should now be TRUE */
    float conf;
    EXPECT_EQ(qreason_get_fact(reasoner, 2, &conf), QREASON_TRUE);
    EXPECT_NEAR(conf, 0.8f * 0.95f, 1e-4f);  /* min(0.9, 0.8) * 0.95 */
}

TEST_F(QuantumReasoningTest, ForwardChainMultipleSteps) {
    /* A -> B, B -> C, C -> D */
    qreason_set_fact(reasoner, 0, QREASON_TRUE, 1.0f);  /* A */

    uint32_t ant0[] = {0};
    uint32_t ant1[] = {1};
    uint32_t ant2[] = {2};
    qreason_add_rule(reasoner, ant0, 1, 1, 0.9f);  /* A -> B */
    qreason_add_rule(reasoner, ant1, 1, 2, 0.9f);  /* B -> C */
    qreason_add_rule(reasoner, ant2, 1, 3, 0.9f);  /* C -> D */

    qreason_result_t result;
    uint32_t new_facts = qreason_forward_chain(reasoner, &result);

    EXPECT_EQ(new_facts, 3u);  /* B, C, D derived */

    float conf;
    EXPECT_EQ(qreason_get_fact(reasoner, 1, &conf), QREASON_TRUE);
    EXPECT_EQ(qreason_get_fact(reasoner, 2, &conf), QREASON_TRUE);
    EXPECT_EQ(qreason_get_fact(reasoner, 3, &conf), QREASON_TRUE);
}

TEST_F(QuantumReasoningTest, ForwardChainLowConfidence) {
    /* A -> B with low confidence, should not trigger if below threshold */
    qreason_set_fact(reasoner, 0, QREASON_TRUE, 0.4f);  /* A with low confidence */

    uint32_t ant[] = {0};
    qreason_add_rule(reasoner, ant, 1, 1, 1.0f);  /* A -> B */

    qreason_result_t result;
    uint32_t new_facts = qreason_forward_chain(reasoner, &result);

    /* Confidence 0.4 * 1.0 = 0.4 < min_confidence (0.5), so should not derive */
    EXPECT_EQ(new_facts, 0u);
}

TEST_F(QuantumReasoningTest, ForwardChainNull) {
    qreason_result_t result;
    EXPECT_EQ(qreason_forward_chain(nullptr, &result), 0u);
    EXPECT_EQ(qreason_forward_chain(reasoner, nullptr), 0u);
}

//=============================================================================
// Query Tests
//=============================================================================

TEST_F(QuantumReasoningTest, Query) {
    qreason_set_fact(reasoner, 0, QREASON_TRUE, 0.9f);
    qreason_set_fact(reasoner, 1, QREASON_FALSE, 0.8f);

    qreason_result_t result;
    int ret = qreason_query(reasoner, 0, &result);

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(result.assignment[0], QREASON_TRUE);
    EXPECT_EQ(result.assignment[1], QREASON_FALSE);
}

TEST_F(QuantumReasoningTest, QueryNull) {
    qreason_result_t result;
    EXPECT_EQ(qreason_query(nullptr, 0, &result), -1);
    EXPECT_EQ(qreason_query(reasoner, QREASON_MAX_VARIABLES, &result), -1);
    EXPECT_EQ(qreason_query(reasoner, 0, nullptr), -1);
}

//=============================================================================
// Consistency Check Tests
//=============================================================================

TEST_F(QuantumReasoningTest, CheckConsistencyValid) {
    qreason_set_fact(reasoner, 0, QREASON_TRUE, 0.9f);
    qreason_set_fact(reasoner, 1, QREASON_FALSE, 0.8f);

    bool consistent = qreason_check_consistency(reasoner);
    EXPECT_TRUE(consistent);
}

TEST_F(QuantumReasoningTest, CheckConsistencyNull) {
    EXPECT_FALSE(qreason_check_consistency(nullptr));
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(QuantumReasoningTest, GetStats) {
    qreason_stats_t qstats;
    int ret = qreason_get_stats(reasoner, &qstats);

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(qstats.queries_performed, 0u);
    EXPECT_EQ(qstats.satisfiable_count, 0u);
    EXPECT_EQ(qstats.unsatisfiable_count, 0u);
}

TEST_F(QuantumReasoningTest, StatsAfterSolving) {
    /* Solve a few SAT problems */
    qreason_cnf_t cnf = {0};
    cnf.n_variables = 2;
    cnf.n_clauses = 1;
    cnf.clauses[0].n_literals = 2;
    cnf.clauses[0].literals[0].variable = 0;
    cnf.clauses[0].literals[0].negated = false;
    cnf.clauses[0].literals[1].variable = 1;
    cnf.clauses[0].literals[1].negated = false;

    qreason_result_t result;
    qreason_solve_sat(reasoner, &cnf, &result);
    qreason_solve_sat(reasoner, &cnf, &result);

    qreason_stats_t stats;
    qreason_get_stats(reasoner, &stats);

    EXPECT_EQ(stats.queries_performed, 2u);
    EXPECT_EQ(stats.satisfiable_count, 2u);
    EXPECT_GT(stats.satisfiability_rate, 0.9f);
}

TEST_F(QuantumReasoningTest, GetStatsNull) {
    qreason_stats_t stats;
    EXPECT_EQ(qreason_get_stats(nullptr, &stats), -1);
    EXPECT_EQ(qreason_get_stats(reasoner, nullptr), -1);
}

//=============================================================================
// Random Number Generation Tests
//=============================================================================

TEST_F(QuantumReasoningTest, RandomNumberGeneration) {
    uint32_t state = 12345;

    uint32_t r1 = qreason_rand(&state);
    uint32_t r2 = qreason_rand(&state);

    EXPECT_NE(r1, r2);
    EXPECT_LE(r1, 0x7FFFu);
    EXPECT_LE(r2, 0x7FFFu);
}

TEST_F(QuantumReasoningTest, RandomFloatRange) {
    uint32_t state = 54321;

    for (int i = 0; i < 100; i++) {
        float r = qreason_randf(&state);
        EXPECT_GE(r, 0.0f);
        EXPECT_LE(r, 1.0f);
    }
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
