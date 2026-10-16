/**
 * @file test_complexity.cpp
 * @brief Tests for the computational complexity theory engine (Google Test)
 */

#include <gtest/gtest.h>

extern "C" {
#include "cognitive/math/nimcp_complexity_theory.h"
}

/* ---------- fixture ---------- */

class ComplexityFixture : public ::testing::Test {
protected:
    void SetUp() override {
        cfg = complexity_theory_default_config();
        sim = complexity_theory_create(&cfg);
        ASSERT_NE(sim, nullptr);
    }
    void TearDown() override {
        complexity_theory_destroy(sim);
    }
    cplx_config_t cfg;
    complexity_theory_sim_t *sim;
};

/* ---------- lifecycle ---------- */

TEST(ComplexityTest, CreateDestroy) {
    cplx_config_t cfg = complexity_theory_default_config();
    complexity_theory_sim_t *sim = complexity_theory_create(&cfg);
    ASSERT_NE(sim, nullptr);
    complexity_theory_destroy(sim);
}

/* ---------- known problem classification ---------- */

TEST_F(ComplexityFixture, SatIsNpComplete) {
    complexity_theory_load_known_problems(sim);

    cplx_class_t cls = complexity_theory_get_class(sim, CPLX_PROB_SAT);
    EXPECT_EQ(cls, CPLX_CLASS_NP_COMPLETE);
    EXPECT_TRUE(complexity_theory_is_np_complete(sim, CPLX_PROB_SAT));
}

TEST_F(ComplexityFixture, HaltingIsUndecidable) {
    complexity_theory_load_known_problems(sim);

    cplx_class_t cls = complexity_theory_get_class(sim, CPLX_PROB_HALTING);
    EXPECT_EQ(cls, CPLX_CLASS_UNDECIDABLE);
    EXPECT_TRUE(complexity_theory_is_undecidable(sim, CPLX_PROB_HALTING));
}

TEST_F(ComplexityFixture, SortingIsInP) {
    complexity_theory_load_known_problems(sim);

    EXPECT_TRUE(complexity_theory_is_in_P(sim, CPLX_PROB_SORTING));
    cplx_class_t cls = complexity_theory_get_class(sim, CPLX_PROB_SORTING);
    EXPECT_EQ(cls, CPLX_CLASS_ONLOGN);
}

/* ---------- Master theorem ---------- */

TEST(ComplexityTest, MasterTheoremMergeSort) {
    /* T(n) = 2T(n/2) + O(n) => O(n log n) */
    cplx_class_t cls = complexity_theory_master_theorem(2, 2, 1.0);
    EXPECT_EQ(cls, CPLX_CLASS_ONLOGN);
}

TEST(ComplexityTest, MasterTheoremBinarySearch) {
    /* T(n) = T(n/2) + O(1) => O(log n) */
    cplx_class_t cls = complexity_theory_master_theorem(1, 2, 0.0);
    EXPECT_EQ(cls, CPLX_CLASS_OLOGN);
}

TEST(ComplexityTest, MasterTheoremStrassen) {
    /* T(n) = 8T(n/2) + O(n^2) => O(n^3) (case 1: a > b^c, 8 > 2^2=4) */
    cplx_class_t cls = complexity_theory_master_theorem(8, 2, 2.0);
    EXPECT_EQ(cls, CPLX_CLASS_ON3);
}

/* ---------- Turing machine ---------- */

TEST(ComplexityTest, TmAcceptsMatchingString) {
    cplx_config_t cfg = complexity_theory_default_config();
    cfg.max_tm_steps = 1000;
    complexity_theory_sim_t *sim = complexity_theory_create(&cfg);
    ASSERT_NE(sim, nullptr);

    /* Simple TM: accepts strings of 'a' (symbol 1) ending with blank (symbol 0)
       State 0: read 1 -> write 1, move right, stay in state 0
       State 0: read 0 -> accept (state 1) */
    complexity_theory_tm_init(sim, 3, 2);  /* 3 states, 2 symbols */
    sim->tm.accept_state = 1;
    sim->tm.reject_state = 2;

    complexity_theory_tm_add_transition(sim, 0, 1, 0, 1, +1); /* read 'a', go right */
    complexity_theory_tm_add_transition(sim, 0, 0, 1, 0, 0);  /* read blank, accept */

    /* Input: "aaa" = {1, 1, 1} */
    uint8_t input[] = {1, 1, 1};
    complexity_theory_tm_load_input(sim, input, 3);

    bool halted = complexity_theory_tm_run(sim, 100);
    EXPECT_TRUE(halted);
    EXPECT_TRUE(complexity_theory_tm_accepted(sim));

    complexity_theory_destroy(sim);
}

/* ---------- more known problems ---------- */

TEST_F(ComplexityFixture, MoreKnownProblems) {
    complexity_theory_load_known_problems(sim);

    /* 3SAT is NP-complete */
    EXPECT_TRUE(complexity_theory_is_np_complete(sim, CPLX_PROB_3SAT));
    /* Clique is NP-complete */
    EXPECT_TRUE(complexity_theory_is_np_complete(sim, CPLX_PROB_CLIQUE));
    /* Shortest path is in P */
    EXPECT_TRUE(complexity_theory_is_in_P(sim, CPLX_PROB_SHORTEST_PATH));
    /* PCP is undecidable */
    EXPECT_TRUE(complexity_theory_is_undecidable(sim, CPLX_PROB_PCP));
}

/* ---------- class names ---------- */

TEST(ComplexityTest, ClassNames) {
    const char *name = complexity_theory_class_name(CPLX_CLASS_P);
    ASSERT_NE(name, nullptr);

    const char *np = complexity_theory_class_name(CPLX_CLASS_NP);
    ASSERT_NE(np, nullptr);

    const char *undec = complexity_theory_class_name(CPLX_CLASS_UNDECIDABLE);
    ASSERT_NE(undec, nullptr);
}

/* ---------- TM rejects ---------- */

TEST(ComplexityTest, TmRejects) {
    cplx_config_t cfg = complexity_theory_default_config();
    cfg.max_tm_steps = 100;
    complexity_theory_sim_t *sim = complexity_theory_create(&cfg);
    ASSERT_NE(sim, nullptr);

    /* TM that only accepts symbol 1; rejects on symbol 2 */
    complexity_theory_tm_init(sim, 3, 3);
    sim->tm.accept_state = 1;
    sim->tm.reject_state = 2;

    complexity_theory_tm_add_transition(sim, 0, 1, 1, 1, 0);  /* accept on 1 */
    complexity_theory_tm_add_transition(sim, 0, 2, 2, 2, 0);  /* reject on 2 */

    uint8_t input[] = {2};
    complexity_theory_tm_load_input(sim, input, 1);

    bool halted = complexity_theory_tm_run(sim, 100);
    EXPECT_TRUE(halted);
    EXPECT_FALSE(complexity_theory_tm_accepted(sim));

    complexity_theory_destroy(sim);
}
