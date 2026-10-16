/**
 * @file test_unification_engine.cpp
 * @brief Unit tests for MODULE 5: Unification Engine
 *
 * TEST COVERAGE: 10 tests
 *
 * @author NIMCP Development Team
 * @date 2025-11-20
 */

#include <gtest/gtest.h>
// Headers have their own extern "C" guards
    #include "cognitive/reasoning/nimcp_unification_engine.h"
    #include "cognitive/reasoning/nimcp_symbolic_logic_attachment.h"
    #include "cognitive/reasoning/nimcp_reasoning_factory.h"
    #include "core/brain/nimcp_brain.h"

class UnificationEngineTest : public ::testing::Test {
protected:
    brain_t brain;

    void SetUp() override {
        brain = brain_create("test_brain", BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION, 10, 5);
        symbolic_logic_t* engine = create_default_symbolic_logic(REASONING_SIZE_SMALL);
        brain_attach_symbolic_logic(brain, engine);
    }

    void TearDown() override {
        if (brain) {
            symbolic_logic_t* engine = brain_detach_symbolic_logic(brain);
            if (engine) symbolic_logic_destroy(engine);
            brain_destroy(brain);
        }
    }
};

TEST_F(UnificationEngineTest, UnifyTermsWithNullBrainFails) {
    logical_term_t term1, term2;
    unification_t result;
    EXPECT_FALSE(brain_unify_terms(nullptr, &term1, &term2, &result));
}

TEST_F(UnificationEngineTest, UnifyTermsWithNullTerm1Fails) {
    logical_term_t term2;
    unification_t result;
    EXPECT_FALSE(brain_unify_terms(brain, nullptr, &term2, &result));
}

TEST_F(UnificationEngineTest, UnifyTermsWithNullTerm2Fails) {
    logical_term_t term1;
    unification_t result;
    EXPECT_FALSE(brain_unify_terms(brain, &term1, nullptr, &result));
}

TEST_F(UnificationEngineTest, UnifyTermsWithNullResultFails) {
    logical_term_t term1, term2;
    EXPECT_FALSE(brain_unify_terms(brain, &term1, &term2, nullptr));
}

TEST_F(UnificationEngineTest, ApplySubstitutionWithNullBrainFails) {
    logical_term_t term;
    unification_t bindings;
    logical_term_t* result = nullptr;
    EXPECT_FALSE(brain_apply_substitution(nullptr, &term, &bindings, &result));
}

TEST_F(UnificationEngineTest, ApplySubstitutionWithNullTermFails) {
    unification_t bindings;
    logical_term_t* result = nullptr;
    EXPECT_FALSE(brain_apply_substitution(brain, nullptr, &bindings, &result));
}

TEST_F(UnificationEngineTest, ApplySubstitutionWithNullBindingsFails) {
    logical_term_t term;
    logical_term_t* result = nullptr;
    EXPECT_FALSE(brain_apply_substitution(brain, &term, nullptr, &result));
}

TEST_F(UnificationEngineTest, ApplySubstitutionWithNullResultFails) {
    logical_term_t term;
    unification_t bindings;
    EXPECT_FALSE(brain_apply_substitution(brain, &term, &bindings, nullptr));
}

TEST_F(UnificationEngineTest, UnificationFreeResultSafeWithNull) {
    unification_free_result(nullptr);
    SUCCEED();
}

TEST_F(UnificationEngineTest, UnificationErrorHandling) {
    const char* error = unification_get_last_error();
    EXPECT_NE(error, nullptr);
}
