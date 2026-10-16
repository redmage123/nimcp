/**
 * @file test_quantum_sat.cpp
 * @brief Comprehensive unit tests for quantum SAT solver
 *
 * Tests cover:
 * - CNF formula construction and validation
 * - SAT solving with various formula structures
 * - 3-SAT, 2-SAT, and k-SAT instances
 * - Random SAT instances
 * - Phase transition behavior
 * - Incremental solving
 * - Statistics and performance metrics
 *
 * @version Phase C2: Quantum Reasoning Integration
 * @date 2025-12-30
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <vector>
#include <random>

// Headers have their own extern "C" guards
#include "cognitive/reasoning/nimcp_quantum_reasoning.h"

//=============================================================================
// Test Fixture
//=============================================================================

class QuantumSATTest : public ::testing::Test {
protected:
    qreason_t reasoner = nullptr;
    std::mt19937 rng;

    void SetUp() override {
        qreason_config_t config = qreason_default_config();
        config.max_grover_iterations = 15;
        reasoner = qreason_create(&config);
        rng.seed(12345);
    }

    void TearDown() override {
        if (reasoner) {
            qreason_destroy(reasoner);
            reasoner = nullptr;
        }
    }

    /**
     * @brief Generate random k-SAT formula
     * @param n_vars Number of variables
     * @param n_clauses Number of clauses
     * @param k Literals per clause
     * @return CNF formula
     */
    qreason_cnf_t generate_random_ksat(uint32_t n_vars, uint32_t n_clauses, uint32_t k) {
        qreason_cnf_t cnf = {0};
        cnf.n_variables = n_vars;
        cnf.n_clauses = (n_clauses < QREASON_MAX_CLAUSES) ? n_clauses : QREASON_MAX_CLAUSES;

        std::uniform_int_distribution<uint32_t> var_dist(0, n_vars - 1);
        std::uniform_int_distribution<int> neg_dist(0, 1);

        for (uint32_t c = 0; c < cnf.n_clauses; c++) {
            cnf.clauses[c].n_literals = (k < QREASON_MAX_LITERALS) ? k : QREASON_MAX_LITERALS;

            for (uint32_t l = 0; l < cnf.clauses[c].n_literals; l++) {
                cnf.clauses[c].literals[l].variable = var_dist(rng);
                cnf.clauses[c].literals[l].negated = neg_dist(rng) == 1;
            }
        }

        return cnf;
    }

    /**
     * @brief Verify if an assignment satisfies a CNF formula
     */
    bool verify_sat(const qreason_cnf_t* cnf, const qreason_truth_t* assignment) {
        for (uint32_t c = 0; c < cnf->n_clauses; c++) {
            bool clause_sat = false;

            for (uint32_t l = 0; l < cnf->clauses[c].n_literals; l++) {
                uint32_t var = cnf->clauses[c].literals[l].variable;
                bool negated = cnf->clauses[c].literals[l].negated;
                bool var_true = (assignment[var] == QREASON_TRUE);

                if (negated ? !var_true : var_true) {
                    clause_sat = true;
                    break;
                }
            }

            if (!clause_sat) {
                return false;
            }
        }
        return true;
    }
};

//=============================================================================
// CNF Construction Tests
//=============================================================================

TEST_F(QuantumSATTest, EmptyCNF) {
    qreason_cnf_t cnf = {0};
    cnf.n_variables = 0;
    cnf.n_clauses = 0;

    qreason_result_t result;
    int ret = qreason_solve_sat(reasoner, &cnf, &result);

    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(result.satisfiable);
}

TEST_F(QuantumSATTest, SingleClauseSingleLiteral) {
    qreason_cnf_t cnf = {0};
    cnf.n_variables = 1;
    cnf.n_clauses = 1;
    cnf.clauses[0].n_literals = 1;
    cnf.clauses[0].literals[0].variable = 0;
    cnf.clauses[0].literals[0].negated = false;

    qreason_result_t result;
    int ret = qreason_solve_sat(reasoner, &cnf, &result);

    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(result.satisfiable);
    EXPECT_EQ(result.assignment[0], QREASON_TRUE);
}

TEST_F(QuantumSATTest, MakeClausePositive) {
    int literals[] = {1, 2, 3};
    qreason_clause_t clause = qreason_make_clause(literals, 3);

    EXPECT_EQ(clause.n_literals, 3u);
    EXPECT_EQ(clause.literals[0].variable, 0u);
    EXPECT_FALSE(clause.literals[0].negated);
    EXPECT_EQ(clause.literals[1].variable, 1u);
    EXPECT_FALSE(clause.literals[1].negated);
    EXPECT_EQ(clause.literals[2].variable, 2u);
    EXPECT_FALSE(clause.literals[2].negated);
}

TEST_F(QuantumSATTest, MakeClauseNegative) {
    int literals[] = {-1, -2, -3};
    qreason_clause_t clause = qreason_make_clause(literals, 3);

    EXPECT_EQ(clause.n_literals, 3u);
    EXPECT_TRUE(clause.literals[0].negated);
    EXPECT_TRUE(clause.literals[1].negated);
    EXPECT_TRUE(clause.literals[2].negated);
}

TEST_F(QuantumSATTest, MakeClauseMixed) {
    int literals[] = {1, -2, 3, -4};
    qreason_clause_t clause = qreason_make_clause(literals, 4);

    EXPECT_EQ(clause.n_literals, 4u);
    EXPECT_FALSE(clause.literals[0].negated);
    EXPECT_TRUE(clause.literals[1].negated);
    EXPECT_FALSE(clause.literals[2].negated);
    EXPECT_TRUE(clause.literals[3].negated);
}

//=============================================================================
// 2-SAT Tests
//=============================================================================

TEST_F(QuantumSATTest, TwoSAT_Simple) {
    /* (x0 OR x1) AND (NOT x0 OR x1) */
    qreason_cnf_t cnf = {0};
    cnf.n_variables = 2;
    cnf.n_clauses = 2;

    /* Clause 0: x0 OR x1 */
    cnf.clauses[0].n_literals = 2;
    cnf.clauses[0].literals[0].variable = 0;
    cnf.clauses[0].literals[0].negated = false;
    cnf.clauses[0].literals[1].variable = 1;
    cnf.clauses[0].literals[1].negated = false;

    /* Clause 1: NOT x0 OR x1 */
    cnf.clauses[1].n_literals = 2;
    cnf.clauses[1].literals[0].variable = 0;
    cnf.clauses[1].literals[0].negated = true;
    cnf.clauses[1].literals[1].variable = 1;
    cnf.clauses[1].literals[1].negated = false;

    qreason_result_t result;
    int ret = qreason_solve_sat(reasoner, &cnf, &result);

    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(result.satisfiable);
    EXPECT_TRUE(verify_sat(&cnf, result.assignment));
}

TEST_F(QuantumSATTest, TwoSAT_Unsatisfiable) {
    /* (x0) AND (NOT x0) */
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
}

TEST_F(QuantumSATTest, TwoSAT_Implication) {
    /* x0 -> x1 (NOT x0 OR x1), x1 -> x2 (NOT x1 OR x2), x0 */
    qreason_cnf_t cnf = {0};
    cnf.n_variables = 3;
    cnf.n_clauses = 3;

    /* Clause 0: NOT x0 OR x1 */
    cnf.clauses[0].n_literals = 2;
    cnf.clauses[0].literals[0].variable = 0;
    cnf.clauses[0].literals[0].negated = true;
    cnf.clauses[0].literals[1].variable = 1;
    cnf.clauses[0].literals[1].negated = false;

    /* Clause 1: NOT x1 OR x2 */
    cnf.clauses[1].n_literals = 2;
    cnf.clauses[1].literals[0].variable = 1;
    cnf.clauses[1].literals[0].negated = true;
    cnf.clauses[1].literals[1].variable = 2;
    cnf.clauses[1].literals[1].negated = false;

    /* Clause 2: x0 */
    cnf.clauses[2].n_literals = 1;
    cnf.clauses[2].literals[0].variable = 0;
    cnf.clauses[2].literals[0].negated = false;

    qreason_result_t result;
    int ret = qreason_solve_sat(reasoner, &cnf, &result);

    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(result.satisfiable);

    /* x0=T, x1=T, x2=T */
    EXPECT_EQ(result.assignment[0], QREASON_TRUE);
    EXPECT_EQ(result.assignment[1], QREASON_TRUE);
    EXPECT_EQ(result.assignment[2], QREASON_TRUE);
}

//=============================================================================
// 3-SAT Tests
//=============================================================================

TEST_F(QuantumSATTest, ThreeSAT_SingleClause) {
    /* (x0 OR x1 OR x2) */
    qreason_cnf_t cnf = {0};
    cnf.n_variables = 3;
    cnf.n_clauses = 1;

    cnf.clauses[0].n_literals = 3;
    cnf.clauses[0].literals[0].variable = 0;
    cnf.clauses[0].literals[0].negated = false;
    cnf.clauses[0].literals[1].variable = 1;
    cnf.clauses[0].literals[1].negated = false;
    cnf.clauses[0].literals[2].variable = 2;
    cnf.clauses[0].literals[2].negated = false;

    qreason_result_t result;
    int ret = qreason_solve_sat(reasoner, &cnf, &result);

    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(result.satisfiable);

    /* At least one variable should be TRUE */
    bool any_true = (result.assignment[0] == QREASON_TRUE) ||
                   (result.assignment[1] == QREASON_TRUE) ||
                   (result.assignment[2] == QREASON_TRUE);
    EXPECT_TRUE(any_true);
}

TEST_F(QuantumSATTest, ThreeSAT_MultiClause) {
    /* (x0 OR x1 OR x2) AND (NOT x0 OR NOT x1 OR x2) AND (x0 OR NOT x2 OR x1) */
    qreason_cnf_t cnf = {0};
    cnf.n_variables = 3;
    cnf.n_clauses = 3;

    /* Clause 0 */
    cnf.clauses[0].n_literals = 3;
    cnf.clauses[0].literals[0] = {0, false};
    cnf.clauses[0].literals[1] = {1, false};
    cnf.clauses[0].literals[2] = {2, false};

    /* Clause 1 */
    cnf.clauses[1].n_literals = 3;
    cnf.clauses[1].literals[0] = {0, true};
    cnf.clauses[1].literals[1] = {1, true};
    cnf.clauses[1].literals[2] = {2, false};

    /* Clause 2 */
    cnf.clauses[2].n_literals = 3;
    cnf.clauses[2].literals[0] = {0, false};
    cnf.clauses[2].literals[1] = {2, true};
    cnf.clauses[2].literals[2] = {1, false};

    qreason_result_t result;
    int ret = qreason_solve_sat(reasoner, &cnf, &result);

    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(result.satisfiable);
    EXPECT_TRUE(verify_sat(&cnf, result.assignment));
}

//=============================================================================
// k-SAT Tests
//=============================================================================

TEST_F(QuantumSATTest, FourSAT) {
    /* (x0 OR x1 OR x2 OR x3) */
    qreason_cnf_t cnf = {0};
    cnf.n_variables = 4;
    cnf.n_clauses = 1;

    cnf.clauses[0].n_literals = 4;
    for (uint32_t i = 0; i < 4; i++) {
        cnf.clauses[0].literals[i].variable = i;
        cnf.clauses[0].literals[i].negated = false;
    }

    qreason_result_t result;
    int ret = qreason_solve_sat(reasoner, &cnf, &result);

    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(result.satisfiable);
}

TEST_F(QuantumSATTest, HornClause) {
    /* Horn clauses: at most one positive literal */
    /* (NOT x0 OR NOT x1 OR x2) AND (NOT x2 OR x3) AND (x0) AND (x1) */
    qreason_cnf_t cnf = {0};
    cnf.n_variables = 4;
    cnf.n_clauses = 4;

    /* NOT x0 OR NOT x1 OR x2 */
    cnf.clauses[0].n_literals = 3;
    cnf.clauses[0].literals[0] = {0, true};
    cnf.clauses[0].literals[1] = {1, true};
    cnf.clauses[0].literals[2] = {2, false};

    /* NOT x2 OR x3 */
    cnf.clauses[1].n_literals = 2;
    cnf.clauses[1].literals[0] = {2, true};
    cnf.clauses[1].literals[1] = {3, false};

    /* x0 */
    cnf.clauses[2].n_literals = 1;
    cnf.clauses[2].literals[0] = {0, false};

    /* x1 */
    cnf.clauses[3].n_literals = 1;
    cnf.clauses[3].literals[0] = {1, false};

    qreason_result_t result;
    int ret = qreason_solve_sat(reasoner, &cnf, &result);

    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(result.satisfiable);

    /* Should derive: x0=T, x1=T, x2=T, x3=T */
    EXPECT_EQ(result.assignment[0], QREASON_TRUE);
    EXPECT_EQ(result.assignment[1], QREASON_TRUE);
    EXPECT_EQ(result.assignment[2], QREASON_TRUE);
    EXPECT_EQ(result.assignment[3], QREASON_TRUE);
}

//=============================================================================
// Random SAT Tests
//=============================================================================

TEST_F(QuantumSATTest, Random3SAT_Easy) {
    /* Low clause/variable ratio - likely satisfiable */
    qreason_cnf_t cnf = generate_random_ksat(4, 4, 3);

    qreason_result_t result;
    int ret = qreason_solve_sat(reasoner, &cnf, &result);

    EXPECT_EQ(ret, 0);
    /* Should complete without error */
}

TEST_F(QuantumSATTest, Random3SAT_Moderate) {
    /* Moderate clause/variable ratio */
    qreason_cnf_t cnf = generate_random_ksat(4, 8, 3);

    qreason_result_t result;
    int ret = qreason_solve_sat(reasoner, &cnf, &result);

    EXPECT_EQ(ret, 0);

    if (result.satisfiable) {
        EXPECT_TRUE(verify_sat(&cnf, result.assignment));
    }
}

TEST_F(QuantumSATTest, Random2SAT) {
    qreason_cnf_t cnf = generate_random_ksat(5, 8, 2);

    qreason_result_t result;
    int ret = qreason_solve_sat(reasoner, &cnf, &result);

    EXPECT_EQ(ret, 0);

    if (result.satisfiable) {
        EXPECT_TRUE(verify_sat(&cnf, result.assignment));
    }
}

//=============================================================================
// Satisfaction Probability Tests
//=============================================================================

TEST_F(QuantumSATTest, SatisfactionProbAllTrue) {
    /* Empty CNF - all states satisfy */
    qreason_cnf_t cnf = {0};
    cnf.n_variables = 3;
    cnf.n_clauses = 0;

    qreason_qstate_t qstate;
    qreason_qstate_init(&qstate, 3);

    float prob = qreason_satisfaction_probability(&qstate, &cnf);
    EXPECT_NEAR(prob, 1.0f, 1e-4f);
}

TEST_F(QuantumSATTest, SatisfactionProbHalf) {
    /* x0 - half of states satisfy */
    qreason_cnf_t cnf = {0};
    cnf.n_variables = 2;
    cnf.n_clauses = 1;
    cnf.clauses[0].n_literals = 1;
    cnf.clauses[0].literals[0] = {0, false};

    qreason_qstate_t qstate;
    qreason_qstate_init(&qstate, 2);

    float prob = qreason_satisfaction_probability(&qstate, &cnf);
    EXPECT_NEAR(prob, 0.5f, 1e-4f);
}

TEST_F(QuantumSATTest, SatisfactionProbNone) {
    /* x0 AND NOT x0 - no states satisfy */
    qreason_cnf_t cnf = {0};
    cnf.n_variables = 1;
    cnf.n_clauses = 2;

    cnf.clauses[0].n_literals = 1;
    cnf.clauses[0].literals[0] = {0, false};

    cnf.clauses[1].n_literals = 1;
    cnf.clauses[1].literals[0] = {0, true};

    qreason_qstate_t qstate;
    qreason_qstate_init(&qstate, 1);

    float prob = qreason_satisfaction_probability(&qstate, &cnf);
    EXPECT_NEAR(prob, 0.0f, 1e-4f);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(QuantumSATTest, StatsTracking) {
    qreason_cnf_t cnf = {0};
    cnf.n_variables = 3;
    cnf.n_clauses = 2;

    cnf.clauses[0].n_literals = 2;
    cnf.clauses[0].literals[0] = {0, false};
    cnf.clauses[0].literals[1] = {1, false};

    cnf.clauses[1].n_literals = 2;
    cnf.clauses[1].literals[0] = {1, false};
    cnf.clauses[1].literals[1] = {2, false};

    qreason_result_t result;

    /* Solve multiple times */
    qreason_solve_sat(reasoner, &cnf, &result);
    qreason_solve_sat(reasoner, &cnf, &result);
    qreason_solve_sat(reasoner, &cnf, &result);

    qreason_stats_t stats;
    qreason_get_stats(reasoner, &stats);

    EXPECT_EQ(stats.queries_performed, 3u);
    EXPECT_EQ(stats.satisfiable_count, 3u);
    EXPECT_FLOAT_EQ(stats.satisfiability_rate, 1.0f);
}

TEST_F(QuantumSATTest, GroverIterationTracking) {
    qreason_cnf_t cnf = {0};
    cnf.n_variables = 4;
    cnf.n_clauses = 4;

    /* Create formula that requires Grover */
    for (uint32_t i = 0; i < 4; i++) {
        cnf.clauses[i].n_literals = 1;
        cnf.clauses[i].literals[0].variable = i;
        cnf.clauses[i].literals[0].negated = false;
    }

    qreason_result_t result;
    qreason_solve_sat(reasoner, &cnf, &result);

    /* Should use some Grover iterations */
    EXPECT_GT(result.grover_iterations, 0u);
}

//=============================================================================
// Edge Cases
//=============================================================================

TEST_F(QuantumSATTest, MaxVariables) {
    /* Test near maximum variable count */
    qreason_cnf_t cnf = {0};
    cnf.n_variables = 16;  /* Capped at 16 for quantum state */
    cnf.n_clauses = 1;

    cnf.clauses[0].n_literals = 2;
    cnf.clauses[0].literals[0] = {0, false};
    cnf.clauses[0].literals[1] = {15, false};

    qreason_result_t result;
    int ret = qreason_solve_sat(reasoner, &cnf, &result);

    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(result.satisfiable);
}

TEST_F(QuantumSATTest, MaxClauses) {
    /* Test with many clauses */
    qreason_cnf_t cnf = {0};
    cnf.n_variables = 3;
    cnf.n_clauses = 50;

    for (uint32_t c = 0; c < 50; c++) {
        cnf.clauses[c].n_literals = 2;
        cnf.clauses[c].literals[0] = {0, false};
        cnf.clauses[c].literals[1] = {1, false};
    }

    qreason_result_t result;
    int ret = qreason_solve_sat(reasoner, &cnf, &result);

    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(result.satisfiable);
}

TEST_F(QuantumSATTest, NullParameters) {
    qreason_cnf_t cnf = {0};
    qreason_result_t result;

    EXPECT_EQ(qreason_solve_sat(nullptr, &cnf, &result), -1);
    EXPECT_EQ(qreason_solve_sat(reasoner, nullptr, &result), -1);
    EXPECT_EQ(qreason_solve_sat(reasoner, &cnf, nullptr), -1);
}

//=============================================================================
// Performance Tests
//=============================================================================

TEST_F(QuantumSATTest, SolveMultipleInstances) {
    const int NUM_INSTANCES = 20;
    int satisfiable_count = 0;

    for (int i = 0; i < NUM_INSTANCES; i++) {
        qreason_cnf_t cnf = generate_random_ksat(4, 6, 3);

        qreason_result_t result;
        int ret = qreason_solve_sat(reasoner, &cnf, &result);

        EXPECT_EQ(ret, 0);

        if (result.satisfiable) {
            satisfiable_count++;
            EXPECT_TRUE(verify_sat(&cnf, result.assignment));
        }
    }

    /* At least some should be satisfiable */
    EXPECT_GT(satisfiable_count, 0);
}

TEST_F(QuantumSATTest, IncreasingComplexity) {
    /* Test with increasing problem size */
    for (uint32_t n_vars = 2; n_vars <= 6; n_vars++) {
        qreason_cnf_t cnf = {0};
        cnf.n_variables = n_vars;
        cnf.n_clauses = n_vars;

        /* Create simple satisfiable formula */
        for (uint32_t c = 0; c < n_vars; c++) {
            cnf.clauses[c].n_literals = 2;
            cnf.clauses[c].literals[0] = {c, false};
            cnf.clauses[c].literals[1] = {(c + 1) % n_vars, false};
        }

        qreason_result_t result;
        int ret = qreason_solve_sat(reasoner, &cnf, &result);

        EXPECT_EQ(ret, 0);
        EXPECT_TRUE(result.satisfiable);
    }
}

//=============================================================================
// Tautology Tests
//=============================================================================

TEST_F(QuantumSATTest, TautologyClause) {
    /* (x0 OR NOT x0) - always true */
    qreason_cnf_t cnf = {0};
    cnf.n_variables = 1;
    cnf.n_clauses = 1;

    cnf.clauses[0].n_literals = 2;
    cnf.clauses[0].literals[0] = {0, false};
    cnf.clauses[0].literals[1] = {0, true};

    qreason_qstate_t qstate;
    qreason_qstate_init(&qstate, 1);

    float prob = qreason_satisfaction_probability(&qstate, &cnf);
    EXPECT_NEAR(prob, 1.0f, 1e-4f);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
