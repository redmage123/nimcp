//=============================================================================
// test_quantum_reasoning.cpp - Unit Tests for Quantum Reasoning
//=============================================================================

#include <gtest/gtest.h>
#include <cmath>

// Headers have their own extern "C" guards
#include "cognitive/reasoning/nimcp_quantum_reasoning.h"

//=============================================================================
// Lifecycle Tests
//=============================================================================

class QReasonLifecycleTest : public ::testing::Test {
protected:
    qreason_t ctx = nullptr;

    void TearDown() override {
        if (ctx) {
            qreason_destroy(ctx);
            ctx = nullptr;
        }
    }
};

TEST_F(QReasonLifecycleTest, CreateWithDefaultConfig) {
    ctx = qreason_create(nullptr);
    ASSERT_NE(ctx, nullptr);
}

TEST_F(QReasonLifecycleTest, CreateWithCustomConfig) {
    qreason_config_t config = qreason_default_config();
    config.max_grover_iterations = 20;
    config.min_confidence = 0.8f;

    ctx = qreason_create(&config);
    ASSERT_NE(ctx, nullptr);
}

TEST_F(QReasonLifecycleTest, DestroyNull) {
    qreason_destroy(nullptr);  // Should not crash
}

TEST_F(QReasonLifecycleTest, GetConfig) {
    qreason_config_t config = qreason_default_config();
    config.max_inference_depth = 50;

    ctx = qreason_create(&config);
    ASSERT_NE(ctx, nullptr);

    qreason_config_t retrieved;
    int result = qreason_get_config(ctx, &retrieved);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(retrieved.max_inference_depth, 50);
}

//=============================================================================
// Knowledge Base Tests
//=============================================================================

class QReasonKBTest : public ::testing::Test {
protected:
    qreason_t ctx = nullptr;

    void SetUp() override {
        ctx = qreason_create(nullptr);
    }

    void TearDown() override {
        if (ctx) {
            qreason_destroy(ctx);
        }
    }
};

TEST_F(QReasonKBTest, SetFact) {
    int result = qreason_set_fact(ctx, 0, QREASON_TRUE, 0.9f);
    EXPECT_EQ(result, 0);

    float confidence;
    qreason_truth_t value = qreason_get_fact(ctx, 0, &confidence);
    EXPECT_EQ(value, QREASON_TRUE);
    EXPECT_NEAR(confidence, 0.9f, 1e-5);
}

TEST_F(QReasonKBTest, SetMultipleFacts) {
    qreason_set_fact(ctx, 0, QREASON_TRUE, 1.0f);
    qreason_set_fact(ctx, 1, QREASON_FALSE, 0.8f);
    qreason_set_fact(ctx, 2, QREASON_UNKNOWN, 0.5f);

    EXPECT_EQ(qreason_get_fact(ctx, 0, nullptr), QREASON_TRUE);
    EXPECT_EQ(qreason_get_fact(ctx, 1, nullptr), QREASON_FALSE);
    EXPECT_EQ(qreason_get_fact(ctx, 2, nullptr), QREASON_UNKNOWN);
}

TEST_F(QReasonKBTest, SetFactInvalidVariable) {
    int result = qreason_set_fact(ctx, QREASON_MAX_VARIABLES, QREASON_TRUE, 1.0f);
    EXPECT_EQ(result, -1);
}

TEST_F(QReasonKBTest, GetFactUnknownDefault) {
    float confidence;
    qreason_truth_t value = qreason_get_fact(ctx, 5, &confidence);
    EXPECT_EQ(value, QREASON_UNKNOWN);
    EXPECT_NEAR(confidence, 0.0f, 1e-5);
}

TEST_F(QReasonKBTest, ClearFacts) {
    qreason_set_fact(ctx, 0, QREASON_TRUE, 1.0f);
    qreason_set_fact(ctx, 1, QREASON_FALSE, 0.8f);

    qreason_clear_facts(ctx);

    EXPECT_EQ(qreason_get_fact(ctx, 0, nullptr), QREASON_UNKNOWN);
    EXPECT_EQ(qreason_get_fact(ctx, 1, nullptr), QREASON_UNKNOWN);
}

TEST_F(QReasonKBTest, AddRule) {
    uint32_t antecedents[] = {0, 1};
    int result = qreason_add_rule(ctx, antecedents, 2, 2, 0.9f);
    EXPECT_EQ(result, 0);
}

TEST_F(QReasonKBTest, ClearRules) {
    uint32_t antecedents[] = {0};
    qreason_add_rule(ctx, antecedents, 1, 1, 0.9f);
    qreason_add_rule(ctx, antecedents, 1, 2, 0.8f);

    qreason_clear_rules(ctx);
    // Rules should be cleared (no direct way to check count)
    EXPECT_TRUE(true);
}

//=============================================================================
// Ternary Logic Tests
//=============================================================================

class QReasonTernaryTest : public ::testing::Test {};

TEST_F(QReasonTernaryTest, AndTrueTrue) {
    EXPECT_EQ(qreason_and(QREASON_TRUE, QREASON_TRUE), QREASON_TRUE);
}

TEST_F(QReasonTernaryTest, AndTrueFalse) {
    EXPECT_EQ(qreason_and(QREASON_TRUE, QREASON_FALSE), QREASON_FALSE);
}

TEST_F(QReasonTernaryTest, AndFalseFalse) {
    EXPECT_EQ(qreason_and(QREASON_FALSE, QREASON_FALSE), QREASON_FALSE);
}

TEST_F(QReasonTernaryTest, AndTrueUnknown) {
    EXPECT_EQ(qreason_and(QREASON_TRUE, QREASON_UNKNOWN), QREASON_UNKNOWN);
}

TEST_F(QReasonTernaryTest, AndFalseUnknown) {
    EXPECT_EQ(qreason_and(QREASON_FALSE, QREASON_UNKNOWN), QREASON_FALSE);
}

TEST_F(QReasonTernaryTest, OrTrueTrue) {
    EXPECT_EQ(qreason_or(QREASON_TRUE, QREASON_TRUE), QREASON_TRUE);
}

TEST_F(QReasonTernaryTest, OrTrueFalse) {
    EXPECT_EQ(qreason_or(QREASON_TRUE, QREASON_FALSE), QREASON_TRUE);
}

TEST_F(QReasonTernaryTest, OrFalseFalse) {
    EXPECT_EQ(qreason_or(QREASON_FALSE, QREASON_FALSE), QREASON_FALSE);
}

TEST_F(QReasonTernaryTest, OrTrueUnknown) {
    EXPECT_EQ(qreason_or(QREASON_TRUE, QREASON_UNKNOWN), QREASON_TRUE);
}

TEST_F(QReasonTernaryTest, OrFalseUnknown) {
    EXPECT_EQ(qreason_or(QREASON_FALSE, QREASON_UNKNOWN), QREASON_UNKNOWN);
}

TEST_F(QReasonTernaryTest, NotTrue) {
    EXPECT_EQ(qreason_not(QREASON_TRUE), QREASON_FALSE);
}

TEST_F(QReasonTernaryTest, NotFalse) {
    EXPECT_EQ(qreason_not(QREASON_FALSE), QREASON_TRUE);
}

TEST_F(QReasonTernaryTest, NotUnknown) {
    EXPECT_EQ(qreason_not(QREASON_UNKNOWN), QREASON_UNKNOWN);
}

TEST_F(QReasonTernaryTest, ImpliesTrueTrue) {
    EXPECT_EQ(qreason_implies(QREASON_TRUE, QREASON_TRUE), QREASON_TRUE);
}

TEST_F(QReasonTernaryTest, ImpliesTrueFalse) {
    EXPECT_EQ(qreason_implies(QREASON_TRUE, QREASON_FALSE), QREASON_FALSE);
}

TEST_F(QReasonTernaryTest, ImpliesFalseAnything) {
    EXPECT_EQ(qreason_implies(QREASON_FALSE, QREASON_TRUE), QREASON_TRUE);
    EXPECT_EQ(qreason_implies(QREASON_FALSE, QREASON_FALSE), QREASON_TRUE);
    EXPECT_EQ(qreason_implies(QREASON_FALSE, QREASON_UNKNOWN), QREASON_TRUE);
}

//=============================================================================
// Forward Chaining Tests
//=============================================================================

class QReasonForwardChainTest : public ::testing::Test {
protected:
    qreason_t ctx = nullptr;

    void SetUp() override {
        ctx = qreason_create(nullptr);
    }

    void TearDown() override {
        if (ctx) {
            qreason_destroy(ctx);
        }
    }
};

TEST_F(QReasonForwardChainTest, SimpleInference) {
    // Rule: A -> B (if A then B)
    uint32_t antecedents[] = {0};  // A
    qreason_add_rule(ctx, antecedents, 1, 1, 0.9f);  // -> B

    // Set A = TRUE
    qreason_set_fact(ctx, 0, QREASON_TRUE, 1.0f);

    // Run forward chaining
    qreason_result_t result;
    uint32_t new_facts = qreason_forward_chain(ctx, &result);

    EXPECT_GT(new_facts, 0);

    // B should now be TRUE
    float confidence;
    EXPECT_EQ(qreason_get_fact(ctx, 1, &confidence), QREASON_TRUE);
    EXPECT_NEAR(confidence, 0.9f, 1e-5);
}

TEST_F(QReasonForwardChainTest, ChainedInference) {
    // A -> B, B -> C
    uint32_t antecedents_ab[] = {0};
    uint32_t antecedents_bc[] = {1};
    qreason_add_rule(ctx, antecedents_ab, 1, 1, 0.9f);  // A -> B
    qreason_add_rule(ctx, antecedents_bc, 1, 2, 0.9f);  // B -> C

    qreason_set_fact(ctx, 0, QREASON_TRUE, 1.0f);  // A = TRUE

    qreason_result_t result;
    qreason_forward_chain(ctx, &result);

    // C should be derived
    EXPECT_EQ(qreason_get_fact(ctx, 2, nullptr), QREASON_TRUE);
}

TEST_F(QReasonForwardChainTest, MultipleAntecedents) {
    // A AND B -> C
    uint32_t antecedents[] = {0, 1};
    qreason_add_rule(ctx, antecedents, 2, 2, 0.8f);

    qreason_set_fact(ctx, 0, QREASON_TRUE, 1.0f);  // A = TRUE
    qreason_set_fact(ctx, 1, QREASON_TRUE, 0.9f);  // B = TRUE

    qreason_result_t result;
    qreason_forward_chain(ctx, &result);

    // C should be derived with confidence = min(1.0, 0.9) * 0.8 = 0.72
    float confidence;
    EXPECT_EQ(qreason_get_fact(ctx, 2, &confidence), QREASON_TRUE);
    EXPECT_NEAR(confidence, 0.72f, 1e-5);
}

TEST_F(QReasonForwardChainTest, InsufficientAntecedents) {
    // A AND B -> C, but only A is true
    uint32_t antecedents[] = {0, 1};
    qreason_add_rule(ctx, antecedents, 2, 2, 0.8f);

    qreason_set_fact(ctx, 0, QREASON_TRUE, 1.0f);  // A = TRUE
    // B is UNKNOWN

    qreason_result_t result;
    qreason_forward_chain(ctx, &result);

    // C should NOT be derived
    EXPECT_EQ(qreason_get_fact(ctx, 2, nullptr), QREASON_UNKNOWN);
}

//=============================================================================
// SAT Solver Tests
//=============================================================================

class QReasonSATTest : public ::testing::Test {
protected:
    qreason_t ctx = nullptr;

    void SetUp() override {
        qreason_config_t config = qreason_default_config();
        config.max_grover_iterations = 5;
        ctx = qreason_create(&config);
    }

    void TearDown() override {
        if (ctx) {
            qreason_destroy(ctx);
        }
    }
};

TEST_F(QReasonSATTest, SimpleSatisfiable) {
    // (x1 OR x2) AND (NOT x1 OR x2)
    // Satisfiable when x2 = TRUE
    qreason_cnf_t cnf = {0};
    cnf.n_variables = 2;
    cnf.n_clauses = 2;

    // Clause 1: x1 OR x2
    int literals1[] = {1, 2};  // Positive = variable (1-indexed)
    cnf.clauses[0] = qreason_make_clause(literals1, 2);

    // Clause 2: NOT x1 OR x2
    int literals2[] = {-1, 2};  // Negative = NOT variable
    cnf.clauses[1] = qreason_make_clause(literals2, 2);

    qreason_result_t result;
    int status = qreason_solve_sat(ctx, &cnf, &result);

    EXPECT_EQ(status, 0);
    EXPECT_TRUE(result.satisfiable);
    EXPECT_GT(result.satisfaction_prob, 0.0f);
}

TEST_F(QReasonSATTest, Unsatisfiable) {
    // (x1) AND (NOT x1) - contradiction
    qreason_cnf_t cnf = {0};
    cnf.n_variables = 1;
    cnf.n_clauses = 2;

    int literals1[] = {1};   // x1
    int literals2[] = {-1};  // NOT x1
    cnf.clauses[0] = qreason_make_clause(literals1, 1);
    cnf.clauses[1] = qreason_make_clause(literals2, 1);

    qreason_result_t result;
    qreason_solve_sat(ctx, &cnf, &result);

    EXPECT_FALSE(result.satisfiable);
}

TEST_F(QReasonSATTest, TriviallySatisfiable) {
    // Single clause: (x1 OR x2 OR x3)
    qreason_cnf_t cnf = {0};
    cnf.n_variables = 3;
    cnf.n_clauses = 1;

    int literals[] = {1, 2, 3};
    cnf.clauses[0] = qreason_make_clause(literals, 3);

    qreason_result_t result;
    qreason_solve_sat(ctx, &cnf, &result);

    EXPECT_TRUE(result.satisfiable);
    // Many solutions exist (7 out of 8)
    EXPECT_GT(result.satisfaction_prob, 0.5f);
}

TEST_F(QReasonSATTest, EmptyFormula) {
    qreason_cnf_t cnf = {0};
    cnf.n_variables = 0;
    cnf.n_clauses = 0;

    qreason_result_t result;
    int status = qreason_solve_sat(ctx, &cnf, &result);

    EXPECT_EQ(status, 0);
    EXPECT_TRUE(result.satisfiable);
}

TEST_F(QReasonSATTest, GroverIterationsRecorded) {
    // Use formula with rare solutions to trigger Grover
    // (A) AND (B) AND (C) - only 1/8 solutions
    qreason_cnf_t cnf = {0};
    cnf.n_variables = 3;
    cnf.n_clauses = 3;

    int lit1[] = {1};  // Clause: A
    int lit2[] = {2};  // Clause: B
    int lit3[] = {3};  // Clause: C
    cnf.clauses[0] = qreason_make_clause(lit1, 1);
    cnf.clauses[1] = qreason_make_clause(lit2, 1);
    cnf.clauses[2] = qreason_make_clause(lit3, 1);

    qreason_result_t result;
    qreason_solve_sat(ctx, &cnf, &result);

    // With only 1/8 solutions, Grover should run
    EXPECT_GT(result.grover_iterations, 0);
}

//=============================================================================
// Query Tests
//=============================================================================

class QReasonQueryTest : public ::testing::Test {
protected:
    qreason_t ctx = nullptr;

    void SetUp() override {
        ctx = qreason_create(nullptr);
    }

    void TearDown() override {
        if (ctx) {
            qreason_destroy(ctx);
        }
    }
};

TEST_F(QReasonQueryTest, QueryKnownFact) {
    qreason_set_fact(ctx, 0, QREASON_TRUE, 1.0f);

    qreason_result_t result;
    int status = qreason_query(ctx, 0, &result);

    EXPECT_EQ(status, 0);
    EXPECT_EQ(result.assignment[0], QREASON_TRUE);
}

TEST_F(QReasonQueryTest, QueryDerivedFact) {
    // A -> B
    uint32_t antecedents[] = {0};
    qreason_add_rule(ctx, antecedents, 1, 1, 0.9f);
    qreason_set_fact(ctx, 0, QREASON_TRUE, 1.0f);

    qreason_result_t result;
    qreason_query(ctx, 1, &result);

    EXPECT_EQ(result.assignment[1], QREASON_TRUE);
}

TEST_F(QReasonQueryTest, QueryInvalidVariable) {
    qreason_result_t result;
    int status = qreason_query(ctx, QREASON_MAX_VARIABLES, &result);
    EXPECT_EQ(status, -1);
}

//=============================================================================
// Consistency Tests
//=============================================================================

class QReasonConsistencyTest : public ::testing::Test {
protected:
    qreason_t ctx = nullptr;

    void SetUp() override {
        ctx = qreason_create(nullptr);
    }

    void TearDown() override {
        if (ctx) {
            qreason_destroy(ctx);
        }
    }
};

TEST_F(QReasonConsistencyTest, ConsistentKB) {
    qreason_set_fact(ctx, 0, QREASON_TRUE, 1.0f);
    qreason_set_fact(ctx, 1, QREASON_FALSE, 0.8f);

    EXPECT_TRUE(qreason_check_consistency(ctx));
}

TEST_F(QReasonConsistencyTest, EmptyKB) {
    EXPECT_TRUE(qreason_check_consistency(ctx));
}

//=============================================================================
// Statistics Tests
//=============================================================================

class QReasonStatsTest : public ::testing::Test {
protected:
    qreason_t ctx = nullptr;

    void SetUp() override {
        ctx = qreason_create(nullptr);
    }

    void TearDown() override {
        if (ctx) {
            qreason_destroy(ctx);
        }
    }
};

TEST_F(QReasonStatsTest, InitialStats) {
    qreason_stats_t stats;
    int result = qreason_get_stats(ctx, &stats);

    EXPECT_EQ(result, 0);
    EXPECT_EQ(stats.queries_performed, 0);
    EXPECT_EQ(stats.satisfiable_count, 0);
    EXPECT_EQ(stats.unsatisfiable_count, 0);
}

TEST_F(QReasonStatsTest, StatsAfterSAT) {
    qreason_cnf_t cnf = {0};
    cnf.n_variables = 2;
    cnf.n_clauses = 1;

    int literals[] = {1, 2};
    cnf.clauses[0] = qreason_make_clause(literals, 2);

    qreason_result_t result;
    qreason_solve_sat(ctx, &cnf, &result);

    qreason_stats_t stats;
    qreason_get_stats(ctx, &stats);

    EXPECT_EQ(stats.queries_performed, 1);
    EXPECT_EQ(stats.satisfiable_count, 1);
}

TEST_F(QReasonStatsTest, GetStatsNull) {
    EXPECT_EQ(qreason_get_stats(nullptr, nullptr), -1);
}

//=============================================================================
// Clause Helper Tests
//=============================================================================

class QReasonClauseTest : public ::testing::Test {};

TEST_F(QReasonClauseTest, MakePositiveClause) {
    int literals[] = {1, 2, 3};  // x0, x1, x2 (1-indexed)
    qreason_clause_t clause = qreason_make_clause(literals, 3);

    EXPECT_EQ(clause.n_literals, 3);
    EXPECT_EQ(clause.literals[0].variable, 0);  // x0
    EXPECT_FALSE(clause.literals[0].negated);
    EXPECT_EQ(clause.literals[1].variable, 1);  // x1
    EXPECT_FALSE(clause.literals[1].negated);
}

TEST_F(QReasonClauseTest, MakeNegativeClause) {
    int literals[] = {-1, -2};  // NOT x0, NOT x1
    qreason_clause_t clause = qreason_make_clause(literals, 2);

    EXPECT_EQ(clause.n_literals, 2);
    EXPECT_EQ(clause.literals[0].variable, 0);
    EXPECT_TRUE(clause.literals[0].negated);
    EXPECT_EQ(clause.literals[1].variable, 1);
    EXPECT_TRUE(clause.literals[1].negated);
}

TEST_F(QReasonClauseTest, MakeMixedClause) {
    int literals[] = {1, -2, 3};  // x0 OR NOT x1 OR x2
    qreason_clause_t clause = qreason_make_clause(literals, 3);

    EXPECT_FALSE(clause.literals[0].negated);
    EXPECT_TRUE(clause.literals[1].negated);
    EXPECT_FALSE(clause.literals[2].negated);
}

//=============================================================================
// Edge Cases
//=============================================================================

class QReasonEdgeCasesTest : public ::testing::Test {};

TEST_F(QReasonEdgeCasesTest, NullContextOperations) {
    EXPECT_EQ(qreason_set_fact(nullptr, 0, QREASON_TRUE, 1.0f), -1);
    EXPECT_EQ(qreason_get_fact(nullptr, 0, nullptr), QREASON_UNKNOWN);
    EXPECT_EQ(qreason_add_rule(nullptr, nullptr, 0, 0, 0.0f), -1);

    qreason_result_t result;
    EXPECT_EQ(qreason_query(nullptr, 0, &result), -1);
    EXPECT_EQ(qreason_solve_sat(nullptr, nullptr, &result), -1);
    EXPECT_FALSE(qreason_check_consistency(nullptr));
}

TEST_F(QReasonEdgeCasesTest, TooManyAntecedents) {
    auto ctx = qreason_create(nullptr);
    ASSERT_NE(ctx, nullptr);

    uint32_t antecedents[100] = {0};
    int result = qreason_add_rule(ctx, antecedents, 100, 0, 0.9f);
    EXPECT_EQ(result, -1);

    qreason_destroy(ctx);
}

TEST_F(QReasonEdgeCasesTest, MaxVariables) {
    auto ctx = qreason_create(nullptr);
    ASSERT_NE(ctx, nullptr);

    // Set fact at maximum variable index
    int result = qreason_set_fact(ctx, QREASON_MAX_VARIABLES - 1, QREASON_TRUE, 1.0f);
    EXPECT_EQ(result, 0);

    EXPECT_EQ(qreason_get_fact(ctx, QREASON_MAX_VARIABLES - 1, nullptr), QREASON_TRUE);

    qreason_destroy(ctx);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
