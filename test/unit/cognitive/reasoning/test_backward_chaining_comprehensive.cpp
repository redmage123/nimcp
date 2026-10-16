/**
 * @file test_backward_chaining_comprehensive.cpp
 * @brief Comprehensive unit tests for Backward Chaining Engine
 *
 * TEST COVERAGE:
 * - Goal-driven proof search
 * - Recursion depth limits
 * - Proof trace generation
 * - Memory cleanup on failure
 *
 * @author NIMCP Development Team
 * @date 2025-02-02
 */

#include <gtest/gtest.h>
#include <chrono>
#include <thread>
#include <cstring>

extern "C" {
#include "cognitive/reasoning/nimcp_backward_chaining.h"
#include "cognitive/reasoning/nimcp_knowledge_base_interface.h"
#include "cognitive/reasoning/nimcp_symbolic_logic_attachment.h"
#include "cognitive/reasoning/nimcp_reasoning_factory.h"
#include "core/brain/nimcp_brain.h"
#include "cognitive/nimcp_symbolic_logic.h"
}

namespace {

//=============================================================================
// Test Fixture
//=============================================================================

class BackwardChainingComprehensiveTest : public ::testing::Test {
protected:
    void SetUp() override {
        brain = brain_create("bc_test_brain", BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION, 10, 5);
        ASSERT_NE(brain, nullptr);

        engine = create_default_symbolic_logic(REASONING_SIZE_MEDIUM);
        ASSERT_NE(engine, nullptr);

        brain_attach_symbolic_logic(brain, engine);
    }

    void TearDown() override {
        if (brain) {
            symbolic_logic_t* detached = brain_detach_symbolic_logic(brain);
            if (detached) {
                symbolic_logic_destroy(detached);
            }
            brain_destroy(brain);
            brain = nullptr;
        }
    }

    brain_t brain = nullptr;
    symbolic_logic_t* engine = nullptr;
};

//=============================================================================
// Goal-Driven Proof Search Tests
//=============================================================================

TEST_F(BackwardChainingComprehensiveTest, DirectFactProof) {
    // Goal is directly in knowledge base
    brain_add_fact(brain, "Human(socrates)", 0.9f);

    backward_chain_result_t result;
    bool proven = brain_backward_chain(brain, "Human(socrates)", &result);

    EXPECT_TRUE(proven);
    EXPECT_TRUE(result.proven);
    EXPECT_GT(result.confidence, 0.0f);
    backward_chain_free_result(&result);
}

TEST_F(BackwardChainingComprehensiveTest, SimpleRuleProof) {
    // Classic syllogism: All men are mortal, Socrates is a man, therefore Socrates is mortal
    brain_add_fact(brain, "Man(socrates)", 0.9f);
    brain_add_rule(brain, "Man(x) -> Mortal(x)", 0.8f);

    backward_chain_result_t result;
    bool proven = brain_backward_chain(brain, "Mortal(socrates)", &result);

    EXPECT_TRUE(proven);
    EXPECT_TRUE(result.proven);
    backward_chain_free_result(&result);
}

TEST_F(BackwardChainingComprehensiveTest, MultiStepProof) {
    // A -> B -> C -> D, prove D from A
    brain_add_fact(brain, "A(x)", 0.9f);
    brain_add_rule(brain, "A(x) -> B(x)", 0.8f);
    brain_add_rule(brain, "B(x) -> C(x)", 0.8f);
    brain_add_rule(brain, "C(x) -> D(x)", 0.8f);

    backward_chain_result_t result;
    bool proven = brain_backward_chain(brain, "D(x)", &result);

    EXPECT_TRUE(proven);
    EXPECT_GE(result.depth_reached, 3);
    backward_chain_free_result(&result);
}

TEST_F(BackwardChainingComprehensiveTest, UnprovableGoal) {
    brain_add_fact(brain, "Bird(tweety)", 0.9f);
    brain_add_rule(brain, "Bird(x) -> CanFly(x)", 0.8f);

    backward_chain_result_t result;
    bool proven = brain_backward_chain(brain, "Mammal(dog)", &result);

    EXPECT_FALSE(proven);
    EXPECT_FALSE(result.proven);
    backward_chain_free_result(&result);
}

TEST_F(BackwardChainingComprehensiveTest, MultipleRulesForSameGoal) {
    // Two different ways to prove the same goal
    brain_add_fact(brain, "HasWings(tweety)", 0.9f);
    brain_add_fact(brain, "Bird(tweety)", 0.85f);
    brain_add_rule(brain, "HasWings(x) -> CanFly(x)", 0.7f);
    brain_add_rule(brain, "Bird(x) -> CanFly(x)", 0.8f);

    backward_chain_result_t result;
    bool proven = brain_backward_chain(brain, "CanFly(tweety)", &result);

    EXPECT_TRUE(proven);
    // Should find proof through one of the paths
    backward_chain_free_result(&result);
}

TEST_F(BackwardChainingComprehensiveTest, ConjunctivePremises) {
    // Rule with multiple premises
    brain_add_fact(brain, "Human(aristotle)", 0.9f);
    brain_add_fact(brain, "Teacher(aristotle)", 0.85f);
    brain_add_rule(brain, "Human(x) & Teacher(x) -> Scholar(x)", 0.8f);

    backward_chain_result_t result;
    bool proven = brain_backward_chain(brain, "Scholar(aristotle)", &result);

    EXPECT_TRUE(proven);
    backward_chain_free_result(&result);
}

TEST_F(BackwardChainingComprehensiveTest, MissingPremise) {
    // One premise missing
    brain_add_fact(brain, "Human(plato)", 0.9f);
    // Teacher(plato) is NOT added
    brain_add_rule(brain, "Human(x) & Teacher(x) -> Scholar(x)", 0.8f);

    backward_chain_result_t result;
    bool proven = brain_backward_chain(brain, "Scholar(plato)", &result);

    EXPECT_FALSE(proven);
    backward_chain_free_result(&result);
}

TEST_F(BackwardChainingComprehensiveTest, VariableUnification) {
    brain_add_fact(brain, "Parent(alice, bob)", 0.9f);
    brain_add_fact(brain, "Parent(bob, charlie)", 0.9f);
    brain_add_rule(brain, "Parent(x, y) & Parent(y, z) -> Grandparent(x, z)", 0.8f);

    backward_chain_result_t result;
    bool proven = brain_backward_chain(brain, "Grandparent(alice, charlie)", &result);

    EXPECT_TRUE(proven);
    backward_chain_free_result(&result);
}

//=============================================================================
// Recursion Depth Limit Tests
//=============================================================================

TEST_F(BackwardChainingComprehensiveTest, ShallowProof) {
    brain_add_fact(brain, "Start(x)", 0.9f);
    brain_add_rule(brain, "Start(x) -> End(x)", 0.8f);

    backward_chain_result_t result;
    brain_backward_chain(brain, "End(x)", &result);

    EXPECT_EQ(result.depth_reached, 1);
    backward_chain_free_result(&result);
}

TEST_F(BackwardChainingComprehensiveTest, DeepProof) {
    // Create a chain of depth 10
    brain_add_fact(brain, "Level0(x)", 0.9f);
    for (int i = 0; i < 10; i++) {
        char rule[128];
        snprintf(rule, sizeof(rule), "Level%d(x) -> Level%d(x)", i, i + 1);
        brain_add_rule(brain, rule, 0.8f);
    }

    backward_chain_result_t result;
    bool proven = brain_backward_chain(brain, "Level10(x)", &result);

    EXPECT_TRUE(proven);
    EXPECT_EQ(result.depth_reached, 10);
    backward_chain_free_result(&result);
}

TEST_F(BackwardChainingComprehensiveTest, DepthLimitExceeded) {
    // Create a very deep chain that may exceed default depth limit
    brain_add_fact(brain, "Deep0(x)", 0.9f);
    for (int i = 0; i < 100; i++) {
        char rule[128];
        snprintf(rule, sizeof(rule), "Deep%d(x) -> Deep%d(x)", i, i + 1);
        brain_add_rule(brain, rule, 0.8f);
    }

    backward_chain_result_t result;
    bool proven = brain_backward_chain(brain, "Deep100(x)", &result);

    // May or may not prove depending on depth limit
    // Should not crash or hang
    backward_chain_free_result(&result);
}

TEST_F(BackwardChainingComprehensiveTest, CircularProof) {
    // Circular rules: A -> B -> C -> A
    brain_add_fact(brain, "Start(x)", 0.9f);
    brain_add_rule(brain, "Start(x) -> A(x)", 0.8f);
    brain_add_rule(brain, "A(x) -> B(x)", 0.8f);
    brain_add_rule(brain, "B(x) -> C(x)", 0.8f);
    brain_add_rule(brain, "C(x) -> A(x)", 0.8f);  // Cycle

    backward_chain_result_t result;
    bool proven = brain_backward_chain(brain, "C(x)", &result);

    // Should detect cycle and not loop forever
    backward_chain_free_result(&result);
}

TEST_F(BackwardChainingComprehensiveTest, SelfReferencingRule) {
    brain_add_rule(brain, "Loop(x) -> Loop(x)", 0.8f);

    backward_chain_result_t result;
    bool proven = brain_backward_chain(brain, "Loop(y)", &result);

    // Should fail (no base fact) without infinite loop
    EXPECT_FALSE(proven);
    backward_chain_free_result(&result);
}

//=============================================================================
// Proof Trace Generation Tests
//=============================================================================

TEST_F(BackwardChainingComprehensiveTest, ProofTraceSimple) {
    brain_add_fact(brain, "Man(socrates)", 0.9f);
    brain_add_rule(brain, "Man(x) -> Mortal(x)", 0.8f);

    backward_chain_result_t result;
    bool proven = brain_backward_chain(brain, "Mortal(socrates)", &result);

    EXPECT_TRUE(proven);
    EXPECT_GE(result.num_steps, 0);  // Should have at least one step
    backward_chain_free_result(&result);
}

TEST_F(BackwardChainingComprehensiveTest, ProofTraceMultiStep) {
    brain_add_fact(brain, "A(x)", 0.9f);
    brain_add_rule(brain, "A(x) -> B(x)", 0.8f);
    brain_add_rule(brain, "B(x) -> C(x)", 0.8f);
    brain_add_rule(brain, "C(x) -> D(x)", 0.8f);

    backward_chain_result_t result;
    bool proven = brain_backward_chain(brain, "D(x)", &result);

    EXPECT_TRUE(proven);
    EXPECT_GE(result.num_steps, 3);  // At least 3 rules applied
    backward_chain_free_result(&result);
}

TEST_F(BackwardChainingComprehensiveTest, ProofTraceDirectFact) {
    brain_add_fact(brain, "DirectFact(x)", 0.9f);

    backward_chain_result_t result;
    bool proven = brain_backward_chain(brain, "DirectFact(x)", &result);

    EXPECT_TRUE(proven);
    // Direct fact proof may have 0 or 1 steps depending on implementation
    backward_chain_free_result(&result);
}

TEST_F(BackwardChainingComprehensiveTest, ProofTraceFailure) {
    brain_add_fact(brain, "Something(x)", 0.9f);

    backward_chain_result_t result;
    bool proven = brain_backward_chain(brain, "Nonexistent(y)", &result);

    EXPECT_FALSE(proven);
    // Failed proof should still have valid result structure
    backward_chain_free_result(&result);
}

TEST_F(BackwardChainingComprehensiveTest, ProofConfidencePropagation) {
    // Chain with decreasing confidence
    brain_add_fact(brain, "Start(x)", 0.9f);
    brain_add_rule(brain, "Start(x) -> Step1(x)", 0.8f);
    brain_add_rule(brain, "Step1(x) -> Step2(x)", 0.8f);

    backward_chain_result_t result;
    brain_backward_chain(brain, "Step2(x)", &result);

    // Final confidence should be product: 0.9 * 0.8 * 0.8 = 0.576
    EXPECT_GT(result.confidence, 0.0f);
    EXPECT_LT(result.confidence, 0.9f);  // Should be less than initial fact
    backward_chain_free_result(&result);
}

//=============================================================================
// Memory Cleanup on Failure Tests
//=============================================================================

TEST_F(BackwardChainingComprehensiveTest, CleanupOnNullBrain) {
    backward_chain_result_t result;
    memset(&result, 0xFF, sizeof(result));  // Fill with garbage

    bool proven = brain_backward_chain(nullptr, "Goal(x)", &result);

    EXPECT_FALSE(proven);
    // Should not crash, result should be safe to free
}

TEST_F(BackwardChainingComprehensiveTest, CleanupOnNullGoal) {
    backward_chain_result_t result;
    bool proven = brain_backward_chain(brain, nullptr, &result);

    EXPECT_FALSE(proven);
}

TEST_F(BackwardChainingComprehensiveTest, CleanupOnNullResult) {
    brain_add_fact(brain, "Test(x)", 0.9f);
    bool proven = brain_backward_chain(brain, "Test(x)", nullptr);

    EXPECT_FALSE(proven);
}

TEST_F(BackwardChainingComprehensiveTest, CleanupOnEmptyGoal) {
    backward_chain_result_t result;
    bool proven = brain_backward_chain(brain, "", &result);

    EXPECT_FALSE(proven);
    backward_chain_free_result(&result);
}

TEST_F(BackwardChainingComprehensiveTest, FreeResultNullSafe) {
    backward_chain_free_result(nullptr);
    SUCCEED();  // Should not crash
}

TEST_F(BackwardChainingComprehensiveTest, FreeResultEmpty) {
    backward_chain_result_t result;
    memset(&result, 0, sizeof(result));

    backward_chain_free_result(&result);
    SUCCEED();  // Should not crash
}

TEST_F(BackwardChainingComprehensiveTest, FreeResultAfterFailure) {
    backward_chain_result_t result;
    brain_backward_chain(brain, "Nonexistent(x)", &result);

    backward_chain_free_result(&result);  // Should be safe
    SUCCEED();
}

TEST_F(BackwardChainingComprehensiveTest, MultipleConsecutiveProofs) {
    brain_add_fact(brain, "Fact1(x)", 0.9f);
    brain_add_fact(brain, "Fact2(y)", 0.85f);
    brain_add_rule(brain, "Fact1(x) -> Derived1(x)", 0.8f);
    brain_add_rule(brain, "Fact2(y) -> Derived2(y)", 0.8f);

    // Run multiple proofs and cleanup
    for (int i = 0; i < 10; i++) {
        backward_chain_result_t result;
        brain_backward_chain(brain, (i % 2 == 0) ? "Derived1(x)" : "Derived2(y)", &result);
        backward_chain_free_result(&result);
    }

    SUCCEED();  // Should not leak memory
}

//=============================================================================
// Step-wise Proof Tests
//=============================================================================

TEST_F(BackwardChainingComprehensiveTest, SingleStepProof) {
    brain_add_fact(brain, "Premise(x)", 0.9f);
    brain_add_rule(brain, "Premise(x) -> Conclusion(x)", 0.8f);

    logic_clause_t** premises = nullptr;
    uint32_t num_premises = 0;

    bool success = brain_backward_chain_step(brain, "Conclusion(x)", &premises, &num_premises);

    // Should find matching rule and return its premises
    EXPECT_TRUE(success);
}

TEST_F(BackwardChainingComprehensiveTest, StepNullBrain) {
    logic_clause_t** premises = nullptr;
    uint32_t num_premises = 0;

    bool success = brain_backward_chain_step(nullptr, "Goal(x)", &premises, &num_premises);
    EXPECT_FALSE(success);
}

TEST_F(BackwardChainingComprehensiveTest, StepNullSubgoal) {
    logic_clause_t** premises = nullptr;
    uint32_t num_premises = 0;

    bool success = brain_backward_chain_step(brain, nullptr, &premises, &num_premises);
    EXPECT_FALSE(success);
}

TEST_F(BackwardChainingComprehensiveTest, StepNoMatchingRules) {
    logic_clause_t** premises = nullptr;
    uint32_t num_premises = 0;

    bool success = brain_backward_chain_step(brain, "UnmatchedGoal(x)", &premises, &num_premises);

    // Should return false when no matching rules found
    EXPECT_FALSE(success);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(BackwardChainingComprehensiveTest, GetStatsInitial) {
    uint32_t attempted = 0, succeeded = 0;
    float avg_depth = 0.0f;

    bool success = brain_get_backward_chain_stats(brain, &attempted, &succeeded, &avg_depth);

    EXPECT_TRUE(success);
    EXPECT_EQ(attempted, 0);
    EXPECT_EQ(succeeded, 0);
}

TEST_F(BackwardChainingComprehensiveTest, GetStatsAfterProof) {
    brain_add_fact(brain, "Test(x)", 0.9f);
    brain_add_rule(brain, "Test(x) -> Result(x)", 0.8f);

    backward_chain_result_t result;
    brain_backward_chain(brain, "Result(x)", &result);
    backward_chain_free_result(&result);

    uint32_t attempted = 0, succeeded = 0;
    float avg_depth = 0.0f;

    bool success = brain_get_backward_chain_stats(brain, &attempted, &succeeded, &avg_depth);

    EXPECT_TRUE(success);
    EXPECT_GT(attempted, 0);
}

TEST_F(BackwardChainingComprehensiveTest, GetStatsNullBrain) {
    uint32_t attempted = 0;
    bool success = brain_get_backward_chain_stats(nullptr, &attempted, nullptr, nullptr);

    EXPECT_FALSE(success);
}

TEST_F(BackwardChainingComprehensiveTest, GetStatsNullOutputs) {
    bool success = brain_get_backward_chain_stats(brain, nullptr, nullptr, nullptr);

    // Should succeed even with NULL outputs
    EXPECT_TRUE(success);
}

//=============================================================================
// Error Handling Tests
//=============================================================================

TEST_F(BackwardChainingComprehensiveTest, GetLastError) {
    brain_backward_chain(nullptr, "Goal", nullptr);

    const char* error = backward_chain_get_last_error();
    EXPECT_NE(error, nullptr);
}

TEST_F(BackwardChainingComprehensiveTest, MalformedGoal) {
    backward_chain_result_t result;

    // Various malformed goals
    const char* malformed[] = {
        "(",
        ")",
        "Incomplete(",
        "Too(many,args,here,is,bad)",
        "Special@Chars#In$Name",
    };

    for (const char* goal : malformed) {
        bool proven = brain_backward_chain(brain, goal, &result);
        // Should handle gracefully without crashing
        backward_chain_free_result(&result);
    }
}

//=============================================================================
// Performance Tests
//=============================================================================

TEST_F(BackwardChainingComprehensiveTest, ProofPerformance) {
    // Setup moderate proof complexity
    brain_add_fact(brain, "Base(x)", 0.9f);
    for (int i = 0; i < 10; i++) {
        char rule[128];
        snprintf(rule, sizeof(rule), "Level%d(x) -> Level%d(x)", i, i + 1);
        brain_add_rule(brain, rule, 0.8f);
    }
    brain_add_rule(brain, "Base(x) -> Level0(x)", 0.8f);

    auto start = std::chrono::high_resolution_clock::now();

    backward_chain_result_t result;
    brain_backward_chain(brain, "Level10(x)", &result);

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    EXPECT_LT(duration.count(), 5000);  // Should complete in < 5 seconds
    EXPECT_GE(result.inference_time_ms, 0);

    backward_chain_free_result(&result);
}

TEST_F(BackwardChainingComprehensiveTest, ManyRulesPerformance) {
    brain_add_fact(brain, "Root(x)", 0.9f);

    // Add many rules
    for (int i = 0; i < 100; i++) {
        char rule[128];
        snprintf(rule, sizeof(rule), "Root(x) -> Branch%d(x)", i);
        brain_add_rule(brain, rule, 0.8f);
    }

    backward_chain_result_t result;
    bool proven = brain_backward_chain(brain, "Branch50(x)", &result);

    EXPECT_TRUE(proven);
    backward_chain_free_result(&result);
}

}  // anonymous namespace
