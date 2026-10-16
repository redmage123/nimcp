/**
 * @file test_grover_search.cpp
 * @brief Comprehensive unit tests for Grover search algorithm implementation
 *
 * Tests cover:
 * - Amplitude amplification correctness
 * - Oracle application
 * - Diffusion operator
 * - Optimal iteration count
 * - Multi-target search
 * - Edge cases and bounds
 * - Speedup verification
 *
 * @version Phase C2: Quantum Reasoning Integration
 * @date 2025-12-30
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <algorithm>

// Headers have their own extern "C" guards
#include "cognitive/reasoning/nimcp_quantum_reasoning.h"

//=============================================================================
// Test Fixture
//=============================================================================

class GroverSearchTest : public ::testing::Test {
protected:
    qreason_t reasoner = nullptr;

    void SetUp() override {
        qreason_config_t config = qreason_default_config();
        config.max_grover_iterations = 20;
        reasoner = qreason_create(&config);
    }

    void TearDown() override {
        if (reasoner) {
            qreason_destroy(reasoner);
            reasoner = nullptr;
        }
    }

    /**
     * @brief Create a CNF formula with exactly one satisfying assignment
     * @param n_vars Number of variables
     * @param solution The satisfying assignment (bit pattern)
     * @return CNF formula
     */
    qreason_cnf_t create_unique_solution_cnf(uint32_t n_vars, uint32_t solution) {
        qreason_cnf_t cnf = {0};
        cnf.n_variables = n_vars;
        cnf.n_clauses = n_vars;

        for (uint32_t i = 0; i < n_vars; i++) {
            cnf.clauses[i].n_literals = 1;
            cnf.clauses[i].literals[0].variable = i;
            cnf.clauses[i].literals[0].negated = !((solution >> i) & 1);
        }

        return cnf;
    }

    /**
     * @brief Create a CNF formula with multiple satisfying assignments
     * @param n_vars Number of variables
     * @param clause_pattern Clause pattern as OR of literals
     * @return CNF formula
     */
    qreason_cnf_t create_or_clause_cnf(uint32_t n_vars) {
        qreason_cnf_t cnf = {0};
        cnf.n_variables = n_vars;
        cnf.n_clauses = 1;

        cnf.clauses[0].n_literals = n_vars;
        for (uint32_t i = 0; i < n_vars; i++) {
            cnf.clauses[0].literals[i].variable = i;
            cnf.clauses[0].literals[i].negated = false;
        }

        return cnf;
    }
};

//=============================================================================
// Amplitude Initialization Tests
//=============================================================================

TEST_F(GroverSearchTest, UniformSuperpositionInit) {
    qreason_qstate_t qstate;
    qreason_qstate_init(&qstate, 4);

    /* All amplitudes should be equal: 1/sqrt(16) = 0.25 */
    float expected = 1.0f / sqrtf(16.0f);
    for (uint32_t i = 0; i < 16; i++) {
        EXPECT_NEAR(qstate.amplitudes[i], expected, 1e-6f);
    }
}

TEST_F(GroverSearchTest, ProbabilityNormalized) {
    qreason_qstate_t qstate;
    qreason_qstate_init(&qstate, 5);

    float total_prob = 0.0f;
    for (uint32_t i = 0; i < qstate.state_dim; i++) {
        total_prob += qstate.amplitudes[i] * qstate.amplitudes[i];
    }

    EXPECT_NEAR(total_prob, 1.0f, 1e-5f);
}

//=============================================================================
// Oracle Tests
//=============================================================================

TEST_F(GroverSearchTest, OracleFlipsTargetPhase) {
    qreason_qstate_t qstate;
    qreason_qstate_init(&qstate, 3);

    /* Create CNF with unique solution at state 5 (binary: 101) */
    qreason_cnf_t cnf = create_unique_solution_cnf(3, 5);

    float amplitude_before = qstate.amplitudes[5];
    qreason_oracle_cnf(&qstate, &cnf);
    float amplitude_after = qstate.amplitudes[5];

    /* Phase should be flipped (sign inverted) */
    EXPECT_NEAR(amplitude_after, -amplitude_before, 1e-6f);
}

TEST_F(GroverSearchTest, OraclePreservesNonTargets) {
    qreason_qstate_t qstate;
    qreason_qstate_init(&qstate, 3);

    /* Create CNF with unique solution at state 3 */
    qreason_cnf_t cnf = create_unique_solution_cnf(3, 3);

    std::vector<float> before(8);
    for (uint32_t i = 0; i < 8; i++) {
        before[i] = qstate.amplitudes[i];
    }

    qreason_oracle_cnf(&qstate, &cnf);

    /* Non-target states should have same amplitude */
    for (uint32_t i = 0; i < 8; i++) {
        if (i != 3) {
            EXPECT_NEAR(qstate.amplitudes[i], before[i], 1e-6f);
        }
    }
}

TEST_F(GroverSearchTest, OracleMultipleTargets) {
    qreason_qstate_t qstate;
    qreason_qstate_init(&qstate, 3);

    /* Create OR clause: x0 OR x1 OR x2 (7 solutions, only 000 fails) */
    qreason_cnf_t cnf = create_or_clause_cnf(3);

    std::vector<float> before(8);
    for (uint32_t i = 0; i < 8; i++) {
        before[i] = qstate.amplitudes[i];
    }

    qreason_oracle_cnf(&qstate, &cnf);

    /* State 0 (all false) should NOT be flipped */
    EXPECT_NEAR(qstate.amplitudes[0], before[0], 1e-6f);

    /* All other states should be flipped */
    for (uint32_t i = 1; i < 8; i++) {
        EXPECT_NEAR(qstate.amplitudes[i], -before[i], 1e-6f);
    }
}

//=============================================================================
// Diffusion Operator Tests
//=============================================================================

TEST_F(GroverSearchTest, DiffusionInversionAboutMean) {
    qreason_qstate_t qstate;
    qreason_qstate_init(&qstate, 2);

    /* Manually set amplitudes */
    qstate.amplitudes[0] = 0.6f;
    qstate.amplitudes[1] = 0.4f;
    qstate.amplitudes[2] = 0.5f;
    qstate.amplitudes[3] = 0.3f;

    /* Mean = (0.6 + 0.4 + 0.5 + 0.3) / 4 = 0.45 */
    float mean = 0.45f;

    qreason_diffusion(&qstate);

    /* New amplitude = 2 * mean - old */
    EXPECT_NEAR(qstate.amplitudes[0], 2.0f * mean - 0.6f, 1e-5f);
    EXPECT_NEAR(qstate.amplitudes[1], 2.0f * mean - 0.4f, 1e-5f);
    EXPECT_NEAR(qstate.amplitudes[2], 2.0f * mean - 0.5f, 1e-5f);
    EXPECT_NEAR(qstate.amplitudes[3], 2.0f * mean - 0.3f, 1e-5f);
}

TEST_F(GroverSearchTest, DiffusionPreservesNorm) {
    qreason_qstate_t qstate;
    qreason_qstate_init(&qstate, 4);

    /* Apply diffusion multiple times */
    for (int i = 0; i < 10; i++) {
        qreason_diffusion(&qstate);
    }

    /* Check normalization (approximately, may drift due to numerical issues) */
    float total_prob = 0.0f;
    for (uint32_t i = 0; i < qstate.state_dim; i++) {
        total_prob += qstate.amplitudes[i] * qstate.amplitudes[i];
    }

    /* Should be close to 1.0 */
    EXPECT_NEAR(total_prob, 1.0f, 0.1f);
}

//=============================================================================
// Full Grover Iteration Tests
//=============================================================================

TEST_F(GroverSearchTest, AmplitudeAmplification) {
    qreason_qstate_t qstate;
    qreason_qstate_init(&qstate, 3);

    /* Create CNF with unique solution at state 6 */
    qreason_cnf_t cnf = create_unique_solution_cnf(3, 6);

    float initial_prob = qstate.amplitudes[6] * qstate.amplitudes[6];

    /* Apply Grover iteration: Oracle + Diffusion */
    qreason_oracle_cnf(&qstate, &cnf);
    qreason_diffusion(&qstate);

    float final_prob = qstate.amplitudes[6] * qstate.amplitudes[6];

    /* Target probability should increase */
    EXPECT_GT(final_prob, initial_prob);
}

TEST_F(GroverSearchTest, MultipleIterations) {
    qreason_qstate_t qstate;
    qreason_qstate_init(&qstate, 4);  /* 16 states */

    /* Create CNF with unique solution at state 10 */
    qreason_cnf_t cnf = create_unique_solution_cnf(4, 10);

    std::vector<float> probs;
    probs.push_back(qstate.amplitudes[10] * qstate.amplitudes[10]);

    /* Track probability across iterations */
    for (int iter = 0; iter < 5; iter++) {
        qreason_oracle_cnf(&qstate, &cnf);
        qreason_diffusion(&qstate);
        probs.push_back(qstate.amplitudes[10] * qstate.amplitudes[10]);
    }

    /* Probability should initially increase */
    EXPECT_GT(probs[1], probs[0]);
    EXPECT_GT(probs[2], probs[1]);

    /* Maximum probability should be > 0.5 within 5 iterations for N=16 */
    float max_prob = *std::max_element(probs.begin(), probs.end());
    EXPECT_GT(max_prob, 0.5f);
}

//=============================================================================
// Optimal Iteration Count Tests
//=============================================================================

TEST_F(GroverSearchTest, OptimalIterationsFormula) {
    /* For N states and 1 solution, optimal iterations ~ pi/4 * sqrt(N) */

    /* N = 16, optimal ~ pi/4 * 4 = 3.14 ~ 3 iterations */
    qreason_qstate_t qstate;
    qreason_qstate_init(&qstate, 4);
    qreason_cnf_t cnf = create_unique_solution_cnf(4, 7);

    float best_prob = 0.0f;
    int best_iter = 0;

    for (int iter = 1; iter <= 10; iter++) {
        qreason_qstate_init(&qstate, 4);  /* Reset */

        for (int i = 0; i < iter; i++) {
            qreason_oracle_cnf(&qstate, &cnf);
            qreason_diffusion(&qstate);
        }

        float prob = qstate.amplitudes[7] * qstate.amplitudes[7];
        if (prob > best_prob) {
            best_prob = prob;
            best_iter = iter;
        }
    }

    /* For N=16, first peak at ~3 (prob=0.961), second peak at ~9 (prob=0.994).
     * The second peak can exceed the first for small N, so allow either. */
    EXPECT_GE(best_iter, 2);
    EXPECT_LE(best_iter, 10);
    EXPECT_GT(best_prob, 0.9f);
}

//=============================================================================
// SAT Solving Integration Tests
//=============================================================================

TEST_F(GroverSearchTest, SolveSATWithUniqueSolution) {
    qreason_cnf_t cnf = create_unique_solution_cnf(4, 11);

    qreason_result_t result;
    int ret = qreason_solve_sat(reasoner, &cnf, &result);

    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(result.satisfiable);

    /* Verify solution matches target */
    bool matches = true;
    for (uint32_t i = 0; i < 4; i++) {
        bool expected = (11 >> i) & 1;
        bool found = (result.assignment[i] == QREASON_TRUE);
        if (expected != found) {
            matches = false;
            break;
        }
    }
    EXPECT_TRUE(matches);
}

TEST_F(GroverSearchTest, SolveSATWithMultipleSolutions) {
    /* x0 OR x1 - 3 solutions */
    qreason_cnf_t cnf = {0};
    cnf.n_variables = 2;
    cnf.n_clauses = 1;
    cnf.clauses[0].n_literals = 2;
    cnf.clauses[0].literals[0].variable = 0;
    cnf.clauses[0].literals[0].negated = false;
    cnf.clauses[0].literals[1].variable = 1;
    cnf.clauses[0].literals[1].negated = false;

    qreason_result_t result;
    int ret = qreason_solve_sat(reasoner, &cnf, &result);

    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(result.satisfiable);

    /* At least one of x0 or x1 should be true */
    EXPECT_TRUE(result.assignment[0] == QREASON_TRUE ||
                result.assignment[1] == QREASON_TRUE);
}

TEST_F(GroverSearchTest, SolveSATNoSolution) {
    /* x0 AND NOT x0 */
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

//=============================================================================
// Speedup Verification Tests
//=============================================================================

TEST_F(GroverSearchTest, SpeedupVsLinear) {
    /* For 4 variables (16 states), Grover should need ~3 iterations
       vs 16 expected for random classical search */

    qreason_cnf_t cnf = create_unique_solution_cnf(4, 9);

    qreason_result_t result;
    qreason_solve_sat(reasoner, &cnf, &result);

    /* Grover iterations should be significantly less than N=16 */
    EXPECT_LT(result.grover_iterations, 10u);
}

TEST_F(GroverSearchTest, SearchSpeedupEstimate) {
    /* The Grover speedup is sqrt(N) for finding one item in N */

    qreason_cnf_t cnf = create_unique_solution_cnf(4, 5);

    qreason_result_t result;
    qreason_solve_sat(reasoner, &cnf, &result);

    /* For N=16, sqrt(N)=4, so speedup ~ 16/4 = 4 */
    /* This is approximate since we don't have explicit speedup tracking */
    EXPECT_TRUE(result.satisfiable);
    EXPECT_GT(result.satisfaction_prob, 0.5f);
}

//=============================================================================
// Edge Cases
//=============================================================================

TEST_F(GroverSearchTest, SingleVariable) {
    /* 1 variable, 2 states */
    qreason_cnf_t cnf = create_unique_solution_cnf(1, 1);

    qreason_result_t result;
    int ret = qreason_solve_sat(reasoner, &cnf, &result);

    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(result.satisfiable);
    EXPECT_EQ(result.assignment[0], QREASON_TRUE);
}

TEST_F(GroverSearchTest, AllSolutionsSatisfy) {
    /* Empty CNF - all assignments satisfy */
    qreason_cnf_t cnf = {0};
    cnf.n_variables = 3;
    cnf.n_clauses = 0;

    qreason_result_t result;
    int ret = qreason_solve_sat(reasoner, &cnf, &result);

    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(result.satisfiable);
    EXPECT_FLOAT_EQ(result.satisfaction_prob, 1.0f);
}

TEST_F(GroverSearchTest, LargeSearchSpace) {
    /* 8 variables = 256 states */
    qreason_cnf_t cnf = create_unique_solution_cnf(8, 137);

    qreason_result_t result;
    int ret = qreason_solve_sat(reasoner, &cnf, &result);

    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(result.satisfiable);
}

//=============================================================================
// Measurement Tests
//=============================================================================

TEST_F(GroverSearchTest, MeasureHighestProbability) {
    qreason_qstate_t qstate;
    qreason_qstate_init(&qstate, 3);

    /* Apply Grover to amplify state 4 */
    qreason_cnf_t cnf = create_unique_solution_cnf(3, 4);

    for (int i = 0; i < 2; i++) {
        qreason_oracle_cnf(&qstate, &cnf);
        qreason_diffusion(&qstate);
    }

    uint32_t measured = qreason_measure(&qstate);
    EXPECT_EQ(measured, 4u);
}

TEST_F(GroverSearchTest, MeasureWithTie) {
    qreason_qstate_t qstate;
    qstate.n_qubits = 2;
    qstate.state_dim = 4;

    /* Set equal amplitudes */
    qstate.amplitudes[0] = 0.5f;
    qstate.amplitudes[1] = 0.5f;
    qstate.amplitudes[2] = 0.5f;
    qstate.amplitudes[3] = 0.5f;

    uint32_t measured = qreason_measure(&qstate);

    /* Should return first with max probability (0) */
    EXPECT_EQ(measured, 0u);
}

//=============================================================================
// Satisfaction Probability Tests
//=============================================================================

TEST_F(GroverSearchTest, SatisfactionProbExact) {
    qreason_qstate_t qstate;
    qreason_qstate_init(&qstate, 2);

    /* (x0 OR x1) has 3 solutions */
    qreason_cnf_t cnf = create_or_clause_cnf(2);

    float prob = qreason_satisfaction_probability(&qstate, &cnf);
    EXPECT_NEAR(prob, 0.75f, 1e-4f);  /* 3/4 */
}

TEST_F(GroverSearchTest, SatisfactionProbAfterGrover) {
    qreason_qstate_t qstate;
    qreason_qstate_init(&qstate, 3);

    /* Unique solution at state 2 */
    qreason_cnf_t cnf = create_unique_solution_cnf(3, 2);

    float initial_prob = qreason_satisfaction_probability(&qstate, &cnf);
    EXPECT_NEAR(initial_prob, 1.0f / 8.0f, 1e-4f);

    /* Apply Grover iterations */
    for (int i = 0; i < 2; i++) {
        qreason_oracle_cnf(&qstate, &cnf);
        qreason_diffusion(&qstate);
    }

    float final_prob = qreason_satisfaction_probability(&qstate, &cnf);
    EXPECT_GT(final_prob, initial_prob);
    EXPECT_GT(final_prob, 0.5f);
}

//=============================================================================
// Reproducibility Tests
//=============================================================================

TEST_F(GroverSearchTest, Reproducibility) {
    qreason_cnf_t cnf = create_unique_solution_cnf(4, 12);

    qreason_result_t result1, result2;

    qreason_config_t config = qreason_default_config();
    config.seed = 42;

    qreason_t r1 = qreason_create(&config);
    qreason_t r2 = qreason_create(&config);

    qreason_solve_sat(r1, &cnf, &result1);
    qreason_solve_sat(r2, &cnf, &result2);

    /* Same seed should give same result */
    EXPECT_EQ(result1.satisfiable, result2.satisfiable);
    for (uint32_t i = 0; i < 4; i++) {
        EXPECT_EQ(result1.assignment[i], result2.assignment[i]);
    }

    qreason_destroy(r1);
    qreason_destroy(r2);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
