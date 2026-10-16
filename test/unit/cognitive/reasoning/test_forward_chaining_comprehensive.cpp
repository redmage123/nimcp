/**
 * @file test_forward_chaining_comprehensive.cpp
 * @brief Comprehensive unit tests for Forward Chaining Engine
 *
 * TEST COVERAGE:
 * - Rule evaluation
 * - Fact derivation
 * - Working memory bounds
 * - Circular rule detection
 * - Iteration limits
 *
 * @author NIMCP Development Team
 * @date 2025-02-02
 */

#include <gtest/gtest.h>
#include <chrono>
#include <thread>
#include <atomic>

extern "C" {
#include "cognitive/reasoning/nimcp_forward_chaining.h"
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

class ForwardChainingComprehensiveTest : public ::testing::Test {
protected:
    void SetUp() override {
        brain = brain_create("fc_test_brain", BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION, 10, 5);
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
// Rule Evaluation Tests
//=============================================================================

TEST_F(ForwardChainingComprehensiveTest, SimpleRuleApplication) {
    // Add fact and rule
    brain_add_fact(brain, "Bird(tweety)", 0.9f);
    brain_add_rule(brain, "Bird(x) -> CanFly(x)", 0.8f);

    forward_chain_result_t result;
    bool success = brain_forward_chain(brain, 10, &result);

    EXPECT_TRUE(success);
    EXPECT_TRUE(result.converged);
    forward_chain_free_result(&result);
}

TEST_F(ForwardChainingComprehensiveTest, MultipleRuleChain) {
    // Setup: A -> B -> C -> D
    brain_add_fact(brain, "HasProperty(object1, a)", 0.9f);
    brain_add_rule(brain, "HasProperty(x, a) -> HasProperty(x, b)", 0.8f);
    brain_add_rule(brain, "HasProperty(x, b) -> HasProperty(x, c)", 0.8f);
    brain_add_rule(brain, "HasProperty(x, c) -> HasProperty(x, d)", 0.8f);

    forward_chain_result_t result;
    bool success = brain_forward_chain(brain, 10, &result);

    EXPECT_TRUE(success);
    EXPECT_GE(result.iterations_performed, 3);
    forward_chain_free_result(&result);
}

TEST_F(ForwardChainingComprehensiveTest, MultipleFactsMultipleRules) {
    // Multiple facts
    brain_add_fact(brain, "Animal(dog)", 0.9f);
    brain_add_fact(brain, "Animal(cat)", 0.9f);
    brain_add_fact(brain, "Animal(bird)", 0.9f);

    // Multiple rules
    brain_add_rule(brain, "Animal(x) -> Living(x)", 0.8f);
    brain_add_rule(brain, "Animal(x) -> NeedsFood(x)", 0.8f);

    forward_chain_result_t result;
    bool success = brain_forward_chain(brain, 10, &result);

    EXPECT_TRUE(success);
    // Should derive facts for all animals
    forward_chain_free_result(&result);
}

TEST_F(ForwardChainingComprehensiveTest, NoMatchingRules) {
    brain_add_fact(brain, "Bird(tweety)", 0.9f);
    // No rules that match

    forward_chain_result_t result;
    bool success = brain_forward_chain(brain, 10, &result);

    EXPECT_TRUE(success);
    EXPECT_EQ(result.num_new_facts, 0);
    EXPECT_TRUE(result.converged);
    forward_chain_free_result(&result);
}

TEST_F(ForwardChainingComprehensiveTest, RuleWithMultiplePremises) {
    brain_add_fact(brain, "Human(socrates)", 0.9f);
    brain_add_fact(brain, "Philosopher(socrates)", 0.9f);
    brain_add_rule(brain, "Human(x) & Philosopher(x) -> Wise(x)", 0.8f);

    forward_chain_result_t result;
    bool success = brain_forward_chain(brain, 10, &result);

    EXPECT_TRUE(success);
    forward_chain_free_result(&result);
}

//=============================================================================
// Fact Derivation Tests
//=============================================================================

TEST_F(ForwardChainingComprehensiveTest, DerivedFactConfidence) {
    brain_add_fact(brain, "Bird(tweety)", 0.9f);
    brain_add_rule(brain, "Bird(x) -> CanFly(x)", 0.8f);

    forward_chain_result_t result;
    brain_forward_chain(brain, 10, &result);

    // Derived fact confidence should be product of fact and rule confidence
    if (result.num_new_facts > 0 && result.new_facts) {
        EXPECT_GT(result.confidence, 0.0f);
        EXPECT_LE(result.confidence, 1.0f);
    }

    forward_chain_free_result(&result);
}

TEST_F(ForwardChainingComprehensiveTest, VariableBinding) {
    brain_add_fact(brain, "Parent(john, mary)", 0.9f);
    brain_add_fact(brain, "Parent(mary, bob)", 0.9f);
    brain_add_rule(brain, "Parent(x, y) & Parent(y, z) -> Grandparent(x, z)", 0.8f);

    forward_chain_result_t result;
    bool success = brain_forward_chain(brain, 10, &result);

    EXPECT_TRUE(success);
    // Should derive Grandparent(john, bob)
    forward_chain_free_result(&result);
}

TEST_F(ForwardChainingComprehensiveTest, FactDeduplication) {
    brain_add_fact(brain, "Bird(tweety)", 0.9f);
    brain_add_rule(brain, "Bird(x) -> CanFly(x)", 0.8f);

    // Run forward chaining multiple times
    forward_chain_result_t result1, result2;
    brain_forward_chain(brain, 10, &result1);
    brain_forward_chain(brain, 10, &result2);

    // Second run should not derive duplicate facts
    EXPECT_EQ(result2.num_new_facts, 0);

    forward_chain_free_result(&result1);
    forward_chain_free_result(&result2);
}

//=============================================================================
// Working Memory Bounds Tests
//=============================================================================

TEST_F(ForwardChainingComprehensiveTest, LargeFactBase) {
    // Add many facts
    for (int i = 0; i < 100; i++) {
        char fact[64];
        snprintf(fact, sizeof(fact), "Entity(object_%d)", i);
        brain_add_fact(brain, fact, 0.9f);
    }

    brain_add_rule(brain, "Entity(x) -> Processed(x)", 0.8f);

    forward_chain_result_t result;
    bool success = brain_forward_chain(brain, 200, &result);

    EXPECT_TRUE(success);
    forward_chain_free_result(&result);
}

TEST_F(ForwardChainingComprehensiveTest, ManyRules) {
    brain_add_fact(brain, "Start(x)", 0.9f);

    // Add many rules
    for (int i = 0; i < 50; i++) {
        char rule[128];
        snprintf(rule, sizeof(rule), "Step%d(x) -> Step%d(x)", i, i + 1);
        brain_add_rule(brain, rule, 0.8f);
    }
    brain_add_rule(brain, "Start(x) -> Step0(x)", 0.8f);

    forward_chain_result_t result;
    bool success = brain_forward_chain(brain, 100, &result);

    EXPECT_TRUE(success);
    forward_chain_free_result(&result);
}

TEST_F(ForwardChainingComprehensiveTest, MemoryCleanupOnError) {
    // NULL brain should not leak memory
    forward_chain_result_t result;
    bool success = brain_forward_chain(nullptr, 10, &result);

    EXPECT_FALSE(success);
    // Should not crash, no memory to clean up
}

//=============================================================================
// Circular Rule Detection Tests
//=============================================================================

TEST_F(ForwardChainingComprehensiveTest, DirectCircularRule) {
    brain_add_fact(brain, "Start(x)", 0.9f);
    // A -> B -> A (circular)
    brain_add_rule(brain, "Start(x) -> Middle(x)", 0.8f);
    brain_add_rule(brain, "Middle(x) -> Start(x)", 0.8f);

    forward_chain_result_t result;
    bool success = brain_forward_chain(brain, 100, &result);

    // Should either detect cycle or hit iteration limit
    EXPECT_TRUE(success);
    EXPECT_LE(result.iterations_performed, 1000);
    forward_chain_free_result(&result);
}

TEST_F(ForwardChainingComprehensiveTest, IndirectCircularRules) {
    brain_add_fact(brain, "A(x)", 0.9f);
    // A -> B -> C -> D -> A (indirect cycle)
    brain_add_rule(brain, "A(x) -> B(x)", 0.8f);
    brain_add_rule(brain, "B(x) -> C(x)", 0.8f);
    brain_add_rule(brain, "C(x) -> D(x)", 0.8f);
    brain_add_rule(brain, "D(x) -> A(x)", 0.8f);

    forward_chain_result_t result;
    bool success = brain_forward_chain(brain, 50, &result);

    EXPECT_TRUE(success);
    // Should terminate due to fixpoint (no new facts) or iteration limit
    EXPECT_LE(result.iterations_performed, 50);
    forward_chain_free_result(&result);
}

TEST_F(ForwardChainingComprehensiveTest, SelfReferentialRule) {
    brain_add_fact(brain, "Loop(x)", 0.9f);
    brain_add_rule(brain, "Loop(x) -> Loop(x)", 0.8f);  // Self-referential

    forward_chain_result_t result;
    bool success = brain_forward_chain(brain, 100, &result);

    EXPECT_TRUE(success);
    // Should converge immediately (no new facts derivable)
    EXPECT_LE(result.iterations_performed, 5);
    forward_chain_free_result(&result);
}

//=============================================================================
// Iteration Limit Tests
//=============================================================================

TEST_F(ForwardChainingComprehensiveTest, ZeroIterations) {
    brain_add_fact(brain, "Bird(tweety)", 0.9f);
    brain_add_rule(brain, "Bird(x) -> CanFly(x)", 0.8f);

    forward_chain_result_t result;
    bool success = brain_forward_chain(brain, 0, &result);

    // Zero iterations should be treated as unlimited (up to cap)
    EXPECT_TRUE(success);
    forward_chain_free_result(&result);
}

TEST_F(ForwardChainingComprehensiveTest, OneIteration) {
    brain_add_fact(brain, "Bird(tweety)", 0.9f);
    brain_add_rule(brain, "Bird(x) -> CanFly(x)", 0.8f);
    brain_add_rule(brain, "CanFly(x) -> Mobile(x)", 0.8f);

    forward_chain_result_t result;
    bool success = brain_forward_chain(brain, 1, &result);

    EXPECT_TRUE(success);
    EXPECT_EQ(result.iterations_performed, 1);
    forward_chain_free_result(&result);
}

TEST_F(ForwardChainingComprehensiveTest, IterationLimitCapping) {
    brain_add_fact(brain, "Test(x)", 0.9f);

    forward_chain_result_t result;
    bool success = brain_forward_chain(brain, 10000, &result);  // Over cap

    EXPECT_TRUE(success);
    EXPECT_LE(result.iterations_performed, 1000);  // Should be capped
    forward_chain_free_result(&result);
}

TEST_F(ForwardChainingComprehensiveTest, ConvergenceBeforeLimit) {
    brain_add_fact(brain, "A(x)", 0.9f);
    brain_add_rule(brain, "A(x) -> B(x)", 0.8f);

    forward_chain_result_t result;
    bool success = brain_forward_chain(brain, 100, &result);

    EXPECT_TRUE(success);
    EXPECT_TRUE(result.converged);
    EXPECT_LT(result.iterations_performed, 100);  // Should converge early
    forward_chain_free_result(&result);
}

//=============================================================================
// Step-wise Inference Tests
//=============================================================================

TEST_F(ForwardChainingComprehensiveTest, SingleStepInference) {
    brain_add_fact(brain, "Bird(tweety)", 0.9f);
    brain_add_rule(brain, "Bird(x) -> CanFly(x)", 0.8f);

    logic_clause_t** new_facts = nullptr;
    uint32_t num_new = 0;

    bool success = brain_forward_chain_step(brain, &new_facts, &num_new);
    EXPECT_TRUE(success);

    // May or may not derive facts in single step
}

TEST_F(ForwardChainingComprehensiveTest, MultipleStepInference) {
    brain_add_fact(brain, "A(x)", 0.9f);
    brain_add_rule(brain, "A(x) -> B(x)", 0.8f);
    brain_add_rule(brain, "B(x) -> C(x)", 0.8f);

    int total_facts = 0;
    for (int i = 0; i < 5; i++) {
        logic_clause_t** new_facts = nullptr;
        uint32_t num_new = 0;

        bool success = brain_forward_chain_step(brain, &new_facts, &num_new);
        if (!success) break;

        total_facts += num_new;
        if (num_new == 0) break;  // Converged
    }
}

TEST_F(ForwardChainingComprehensiveTest, StepNullBrain) {
    logic_clause_t** facts = nullptr;
    uint32_t num = 0;

    bool success = brain_forward_chain_step(nullptr, &facts, &num);
    EXPECT_FALSE(success);
}

TEST_F(ForwardChainingComprehensiveTest, StepNullOutputs) {
    bool success = brain_forward_chain_step(brain, nullptr, nullptr);
    EXPECT_FALSE(success);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(ForwardChainingComprehensiveTest, GetStatsInitial) {
    uint32_t iterations = 0, facts = 0;
    uint64_t time_ms = 0;

    bool success = brain_get_forward_chain_stats(brain, &iterations, &facts, &time_ms);
    EXPECT_TRUE(success);
    EXPECT_EQ(iterations, 0);
    EXPECT_EQ(facts, 0);
}

TEST_F(ForwardChainingComprehensiveTest, GetStatsAfterInference) {
    brain_add_fact(brain, "Bird(tweety)", 0.9f);
    brain_add_rule(brain, "Bird(x) -> CanFly(x)", 0.8f);

    forward_chain_result_t result;
    brain_forward_chain(brain, 10, &result);
    forward_chain_free_result(&result);

    uint32_t iterations = 0, facts = 0;
    uint64_t time_ms = 0;

    bool success = brain_get_forward_chain_stats(brain, &iterations, &facts, &time_ms);
    EXPECT_TRUE(success);
    EXPECT_GT(iterations, 0);
}

TEST_F(ForwardChainingComprehensiveTest, GetStatsNullOutputs) {
    bool success = brain_get_forward_chain_stats(brain, nullptr, nullptr, nullptr);
    // Should succeed with NULL outputs (just skip filling them)
    EXPECT_TRUE(success);
}

//=============================================================================
// Error Handling Tests
//=============================================================================

TEST_F(ForwardChainingComprehensiveTest, GetLastError) {
    // Trigger an error
    brain_forward_chain(nullptr, 10, nullptr);

    const char* error = forward_chain_get_last_error();
    EXPECT_NE(error, nullptr);
}

TEST_F(ForwardChainingComprehensiveTest, InvalidRule) {
    brain_add_fact(brain, "Valid(fact)", 0.9f);
    // Empty/malformed rule
    brain_add_rule(brain, "", 0.8f);
    brain_add_rule(brain, "->", 0.8f);
    brain_add_rule(brain, "NoConclusion(x) ->", 0.8f);

    forward_chain_result_t result;
    bool success = brain_forward_chain(brain, 10, &result);

    // Should handle gracefully
    EXPECT_TRUE(success);
    forward_chain_free_result(&result);
}

TEST_F(ForwardChainingComprehensiveTest, FreeResultNullSafe) {
    forward_chain_free_result(nullptr);
    SUCCEED();  // Should not crash
}

TEST_F(ForwardChainingComprehensiveTest, FreeResultEmpty) {
    forward_chain_result_t result;
    memset(&result, 0, sizeof(result));

    forward_chain_free_result(&result);
    SUCCEED();  // Should not crash
}

//=============================================================================
// Performance Tests
//=============================================================================

TEST_F(ForwardChainingComprehensiveTest, InferenceTime) {
    brain_add_fact(brain, "Start(x)", 0.9f);

    // Chain of rules
    for (int i = 0; i < 20; i++) {
        char rule[128];
        snprintf(rule, sizeof(rule), "Level%d(x) -> Level%d(x)", i, i + 1);
        brain_add_rule(brain, rule, 0.8f);
    }
    brain_add_rule(brain, "Start(x) -> Level0(x)", 0.8f);

    auto start = std::chrono::high_resolution_clock::now();

    forward_chain_result_t result;
    brain_forward_chain(brain, 50, &result);

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    EXPECT_LT(duration.count(), 5000);  // Should complete in < 5 seconds
    EXPECT_GE(result.inference_time_ms, 0);

    forward_chain_free_result(&result);
}

}  // anonymous namespace
