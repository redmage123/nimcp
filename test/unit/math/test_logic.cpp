/**
 * @file test_logic.cpp
 * @brief Tests for the propositional and predicate logic engine (Google Test)
 */

#include <gtest/gtest.h>

extern "C" {
#include "cognitive/math/nimcp_logic.h"
}

/* ---------- lifecycle ---------- */

TEST(LogicTest, CreateDestroy) {
    logic_engine_t *eng = logic_engine_create();
    ASSERT_NE(eng, nullptr);
    logic_engine_destroy(eng);
}

/* ---------- formula construction ---------- */

TEST(LogicTest, FormulaCreation) {
    logic_formula_t *p = logic_var(0);
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->op, LOGIC_OP_VAR);
    EXPECT_EQ(p->var_index, 0);

    logic_formula_t *t = logic_const(true);
    ASSERT_NE(t, nullptr);
    EXPECT_EQ(t->op, LOGIC_OP_CONST);
    EXPECT_TRUE(t->const_value);

    logic_formula_free(p);
    logic_formula_free(t);
}

/* ---------- tautology: P v ~P = true ---------- */

TEST(LogicTest, TautologyPOrNotP) {
    logic_formula_t *p = logic_var(0);
    logic_formula_t *not_p = logic_not(logic_formula_clone(p));
    logic_formula_t *f = logic_binary(LOGIC_OP_OR, p, not_p);

    EXPECT_TRUE(logic_is_tautology(f, 1));
    logic_formula_free(f);
}

/* ---------- satisfiable: P ^ Q ---------- */

TEST(LogicTest, SatisfiablePAndQ) {
    logic_formula_t *p = logic_var(0);
    logic_formula_t *q = logic_var(1);
    logic_formula_t *f = logic_binary(LOGIC_OP_AND, p, q);

    EXPECT_TRUE(logic_is_satisfiable(f, 2));
    /* Not a tautology: false when P=false or Q=false */
    EXPECT_FALSE(logic_is_tautology(f, 2));
    logic_formula_free(f);
}

/* ---------- unsatisfiable: P ^ ~P = false ---------- */

TEST(LogicTest, UnsatisfiablePAndNotP) {
    logic_formula_t *p = logic_var(0);
    logic_formula_t *not_p = logic_not(logic_formula_clone(p));
    logic_formula_t *f = logic_binary(LOGIC_OP_AND, p, not_p);

    EXPECT_FALSE(logic_is_satisfiable(f, 1));
    logic_formula_free(f);
}

/* ---------- modus ponens ---------- */

TEST(LogicTest, ModusPonensValid) {
    /* Premises: P, P->Q.  Conclusion: Q */
    logic_formula_t *p = logic_var(0);
    logic_formula_t *q = logic_var(1);
    logic_formula_t *p_implies_q = logic_binary(LOGIC_OP_IMPLIES,
                                                 logic_formula_clone(p),
                                                 logic_formula_clone(q));

    const logic_formula_t *premises[2] = { p, p_implies_q };
    bool valid = logic_verify_inference(premises, 2, q, RULE_MODUS_PONENS, 2);
    EXPECT_TRUE(valid);

    logic_formula_free(p);
    logic_formula_free(q);
    logic_formula_free(p_implies_q);
}

/* ---------- evaluation ---------- */

TEST(LogicTest, EvaluateFormula) {
    /* P AND Q: true when both true */
    logic_formula_t *p = logic_var(0);
    logic_formula_t *q = logic_var(1);
    logic_formula_t *f = logic_binary(LOGIC_OP_AND,
                                       logic_formula_clone(p),
                                       logic_formula_clone(q));

    bool assign_tt[] = {true, true};
    bool assign_tf[] = {true, false};
    bool assign_ff[] = {false, false};

    EXPECT_TRUE(logic_evaluate(f, assign_tt));
    EXPECT_FALSE(logic_evaluate(f, assign_tf));
    EXPECT_FALSE(logic_evaluate(f, assign_ff));

    logic_formula_free(f);
    logic_formula_free(p);
    logic_formula_free(q);
}

/* ---------- truth table ---------- */

TEST(LogicTest, TruthTable) {
    logic_formula_t *p = logic_var(0);
    logic_formula_t *not_p = logic_not(logic_formula_clone(p));
    logic_formula_t *f = logic_binary(LOGIC_OP_OR, p, not_p);

    truth_table_t *tt = logic_truth_table(f, 1);
    ASSERT_NE(tt, nullptr);
    EXPECT_EQ(tt->n_variables, 1);
    EXPECT_EQ(tt->n_rows, 2);
    /* All rows should be true (tautology) */
    EXPECT_TRUE(tt->results[0]);
    EXPECT_TRUE(tt->results[1]);

    logic_truth_table_free(tt);
    logic_formula_free(f);
}

/* ---------- equivalence ---------- */

TEST(LogicTest, EquivalenceDeMorgan) {
    /* De Morgan: ~(P^Q) <=> (~P v ~Q) */
    logic_formula_t *p = logic_var(0);
    logic_formula_t *q = logic_var(1);

    logic_formula_t *lhs = logic_not(
        logic_binary(LOGIC_OP_AND,
                     logic_formula_clone(p),
                     logic_formula_clone(q)));

    logic_formula_t *rhs = logic_binary(LOGIC_OP_OR,
        logic_not(logic_formula_clone(p)),
        logic_not(logic_formula_clone(q)));

    EXPECT_TRUE(logic_are_equivalent(lhs, rhs, 2));

    logic_formula_free(lhs);
    logic_formula_free(rhs);
    logic_formula_free(p);
    logic_formula_free(q);
}

/* ---------- implication tautology ---------- */

TEST(LogicTest, ImplicationTautology) {
    /* P -> P is a tautology */
    logic_formula_t *p = logic_var(0);
    logic_formula_t *f = logic_binary(LOGIC_OP_IMPLIES,
                                       logic_formula_clone(p),
                                       logic_formula_clone(p));
    EXPECT_TRUE(logic_is_tautology(f, 1));
    logic_formula_free(f);
    logic_formula_free(p);
}

/* ---------- modus tollens ---------- */

TEST(LogicTest, ModusTollensValid) {
    /* Premises: ~Q, P->Q.  Conclusion: ~P */
    logic_formula_t *p = logic_var(0);
    logic_formula_t *q = logic_var(1);
    logic_formula_t *not_q = logic_not(logic_formula_clone(q));
    logic_formula_t *p_implies_q = logic_binary(LOGIC_OP_IMPLIES,
                                                 logic_formula_clone(p),
                                                 logic_formula_clone(q));
    logic_formula_t *not_p = logic_not(logic_formula_clone(p));

    const logic_formula_t *premises[2] = { not_q, p_implies_q };
    bool valid = logic_verify_inference(premises, 2, not_p, RULE_MODUS_TOLLENS, 2);
    EXPECT_TRUE(valid);

    logic_formula_free(p);
    logic_formula_free(q);
    logic_formula_free(not_q);
    logic_formula_free(p_implies_q);
    logic_formula_free(not_p);
}
