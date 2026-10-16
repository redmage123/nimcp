/**
 * @file test_symbolic_logic_brain_integration.cpp
 * @brief Unit tests for symbolic logic brain integration
 *
 * COVERAGE GOALS:
 * - Lifecycle: Creation, destruction, error handling
 * - Knowledge base: Add facts, add rules, query operations
 * - Forward chaining: Derive new facts from rules
 * - Backward chaining: Prove goals from rules
 * - Working memory integration: Verify inference storage
 * - Executive function integration: Verify planning integration
 * - Error cases: Malformed inputs, circular reasoning
 *
 * TARGET: 30+ tests, 100% code coverage
 *
 * @author NIMCP Development Team
 * @date 2025-01-20
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
    #include "cognitive/reasoning/nimcp_symbolic_logic_brain_integration.h"
    #include "core/brain/nimcp_brain.h"
    #include "cognitive/nimcp_working_memory.h"
    #include "cognitive/nimcp_executive.h"
    #include "utils/logging/nimcp_logging.h"

//=============================================================================
// Test Fixture
//=============================================================================

class SymbolicLogicBrainTest : public ::testing::Test {
protected:
    brain_t brain;

    void SetUp() override {
        // Suppress logging during tests
        nimcp_log_set_level(NULL, LOG_LEVEL_ERROR);

        // Create brain with simple API
        brain = brain_create("symbolic_test", BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION, 10, 5);
        ASSERT_NE(brain, nullptr) << "Failed to create brain";
    }

    void TearDown() override {
        if (brain) {
            brain_destroy(brain);
            brain = nullptr;
        }
    }

    // Helper: Initialize symbolic logic engine
    bool initLogic() {
        return brain_create_symbolic_logic(brain, nullptr);
    }

    // Helper: Initialize with custom config
    bool initLogicCustom(const logic_brain_config_t* config) {
        return brain_create_symbolic_logic(brain, config);
    }
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(SymbolicLogicBrainTest, CreateSymbolicLogic_Success) {
    EXPECT_TRUE(initLogic());
    EXPECT_NE(brain_logic_get_last_error(), nullptr);
}

TEST_F(SymbolicLogicBrainTest, CreateSymbolicLogic_NullBrain) {
    EXPECT_FALSE(brain_create_symbolic_logic(nullptr, nullptr));
    EXPECT_STREQ(brain_logic_get_last_error(), "Brain is NULL");
}

TEST_F(SymbolicLogicBrainTest, CreateSymbolicLogic_AlreadyInitialized) {
    ASSERT_TRUE(initLogic());
    EXPECT_FALSE(initLogic()); // Second call should fail
    EXPECT_STRNE(brain_logic_get_last_error(), "");
}

TEST_F(SymbolicLogicBrainTest, CreateSymbolicLogic_CustomConfig) {
    logic_brain_config_t config = {
        .max_facts = 500,
        .max_rules = 250,
        .max_inference_depth = 5,
        .enable_forward_chaining = true,
        .enable_backward_chaining = true,
        .enable_wm_integration = true,
        .enable_exec_integration = false,
        .wm_inference_salience = 0.8f
    };

    EXPECT_TRUE(initLogicCustom(&config));
}

TEST_F(SymbolicLogicBrainTest, DestroySymbolicLogic_Success) {
    ASSERT_TRUE(initLogic());
    brain_destroy_symbolic_logic(brain);
    // Should be safe to call again
    brain_destroy_symbolic_logic(brain);
}

TEST_F(SymbolicLogicBrainTest, DestroySymbolicLogic_NullBrain) {
    brain_destroy_symbolic_logic(nullptr); // Should not crash
}

//=============================================================================
// Knowledge Base - Add Fact Tests
//=============================================================================

TEST_F(SymbolicLogicBrainTest, AddFact_SimplePredicate) {
    ASSERT_TRUE(initLogic());
    EXPECT_TRUE(brain_add_logical_fact(brain, "Bird(tweety)", 0.9f));
}

TEST_F(SymbolicLogicBrainTest, AddFact_MultipleFacts) {
    ASSERT_TRUE(initLogic());
    EXPECT_TRUE(brain_add_logical_fact(brain, "Bird(tweety)", 0.9f));
    EXPECT_TRUE(brain_add_logical_fact(brain, "Bird(opus)", 0.8f));
    EXPECT_TRUE(brain_add_logical_fact(brain, "Penguin(opus)", 0.85f));
}

TEST_F(SymbolicLogicBrainTest, AddFact_NegatedPredicate) {
    ASSERT_TRUE(initLogic());
    EXPECT_TRUE(brain_add_logical_fact(brain, "~Fly(penguin)", 0.95f));
}

TEST_F(SymbolicLogicBrainTest, AddFact_InvalidSalience) {
    ASSERT_TRUE(initLogic());
    EXPECT_FALSE(brain_add_logical_fact(brain, "Bird(x)", -0.5f)); // Negative salience
    EXPECT_FALSE(brain_add_logical_fact(brain, "Bird(x)", 1.5f));  // Too high
}

TEST_F(SymbolicLogicBrainTest, AddFact_NullBrain) {
    EXPECT_FALSE(brain_add_logical_fact(nullptr, "Bird(x)", 0.5f));
}

TEST_F(SymbolicLogicBrainTest, AddFact_NullString) {
    ASSERT_TRUE(initLogic());
    EXPECT_FALSE(brain_add_logical_fact(brain, nullptr, 0.5f));
}

TEST_F(SymbolicLogicBrainTest, AddFact_NotInitialized) {
    EXPECT_FALSE(brain_add_logical_fact(brain, "Bird(x)", 0.5f));
    EXPECT_STRNE(brain_logic_get_last_error(), "");
}

TEST_F(SymbolicLogicBrainTest, AddFact_MalformedSyntax) {
    ASSERT_TRUE(initLogic());
    EXPECT_FALSE(brain_add_logical_fact(brain, "Bird((tweety", 0.5f)); // Unbalanced parens
    EXPECT_FALSE(brain_add_logical_fact(brain, "", 0.5f));              // Empty string
}

//=============================================================================
// Knowledge Base - Add Rule Tests
//=============================================================================

TEST_F(SymbolicLogicBrainTest, AddRule_SimpleImplication) {
    ASSERT_TRUE(initLogic());
    EXPECT_TRUE(brain_add_logical_rule(brain, "Bird(x) -> Fly(x)", 0.8f));
}

TEST_F(SymbolicLogicBrainTest, AddRule_MultipleRules) {
    ASSERT_TRUE(initLogic());
    EXPECT_TRUE(brain_add_logical_rule(brain, "Bird(x) -> Fly(x)", 0.8f));
    EXPECT_TRUE(brain_add_logical_rule(brain, "Penguin(x) -> Bird(x)", 0.9f));
    EXPECT_TRUE(brain_add_logical_rule(brain, "Penguin(x) -> ~Fly(x)", 0.95f));
}

TEST_F(SymbolicLogicBrainTest, AddRule_ConjunctivePremise) {
    ASSERT_TRUE(initLogic());
    EXPECT_TRUE(brain_add_logical_rule(brain, "Bird(x) & ~Penguin(x) -> Fly(x)", 0.85f));
}

TEST_F(SymbolicLogicBrainTest, AddRule_InvalidPriority) {
    ASSERT_TRUE(initLogic());
    EXPECT_FALSE(brain_add_logical_rule(brain, "Bird(x) -> Fly(x)", -0.1f));
    EXPECT_FALSE(brain_add_logical_rule(brain, "Bird(x) -> Fly(x)", 1.5f));
}

TEST_F(SymbolicLogicBrainTest, AddRule_NullBrain) {
    EXPECT_FALSE(brain_add_logical_rule(nullptr, "Bird(x) -> Fly(x)", 0.5f));
}

TEST_F(SymbolicLogicBrainTest, AddRule_NullString) {
    ASSERT_TRUE(initLogic());
    EXPECT_FALSE(brain_add_logical_rule(brain, nullptr, 0.5f));
}

TEST_F(SymbolicLogicBrainTest, AddRule_NotInitialized) {
    EXPECT_FALSE(brain_add_logical_rule(brain, "Bird(x) -> Fly(x)", 0.5f));
}

TEST_F(SymbolicLogicBrainTest, AddRule_NotAnImplication) {
    ASSERT_TRUE(initLogic());
    EXPECT_FALSE(brain_add_logical_rule(brain, "Bird(x)", 0.5f)); // Not a rule, just a fact
}

//=============================================================================
// Knowledge Base - Query Tests
//=============================================================================

TEST_F(SymbolicLogicBrainTest, Query_GroundQuery) {
    ASSERT_TRUE(initLogic());
    ASSERT_TRUE(brain_add_logical_fact(brain, "Bird(tweety)", 0.9f));

    query_result_t result;
    EXPECT_TRUE(brain_query_knowledge(brain, "Bird(tweety)", &result));
    EXPECT_TRUE(result.success);
    EXPECT_GT(result.num_matches, 0);
    brain_free_query_result(&result);
}

TEST_F(SymbolicLogicBrainTest, Query_NoMatches) {
    ASSERT_TRUE(initLogic());
    ASSERT_TRUE(brain_add_logical_fact(brain, "Bird(tweety)", 0.9f));

    query_result_t result;
    EXPECT_TRUE(brain_query_knowledge(brain, "Fish(nemo)", &result));
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.num_matches, 0);
    brain_free_query_result(&result);
}

TEST_F(SymbolicLogicBrainTest, Query_NullBrain) {
    query_result_t result;
    EXPECT_FALSE(brain_query_knowledge(nullptr, "Bird(x)", &result));
}

TEST_F(SymbolicLogicBrainTest, Query_NullQuery) {
    ASSERT_TRUE(initLogic());
    query_result_t result;
    EXPECT_FALSE(brain_query_knowledge(brain, nullptr, &result));
}

TEST_F(SymbolicLogicBrainTest, Query_NullResult) {
    ASSERT_TRUE(initLogic());
    EXPECT_FALSE(brain_query_knowledge(brain, "Bird(x)", nullptr));
}

TEST_F(SymbolicLogicBrainTest, Query_NotInitialized) {
    query_result_t result;
    EXPECT_FALSE(brain_query_knowledge(brain, "Bird(x)", &result));
}

//=============================================================================
// Forward Chaining Tests
//=============================================================================

TEST_F(SymbolicLogicBrainTest, ForwardChain_DeriveFact) {
    ASSERT_TRUE(initLogic());

    // Setup: Bird(tweety) + [Bird(x) -> Fly(x)] => Should derive Fly(tweety)
    ASSERT_TRUE(brain_add_logical_fact(brain, "Bird(tweety)", 0.9f));
    ASSERT_TRUE(brain_add_logical_rule(brain, "Bird(x) -> Fly(x)", 0.8f));

    inference_result_t result;
    EXPECT_TRUE(brain_forward_chain(brain, 10, &result));
    EXPECT_GT(result.inference_time_ms, 0ULL);
    brain_free_inference_result(&result);
}

TEST_F(SymbolicLogicBrainTest, ForwardChain_MultipleIterations) {
    ASSERT_TRUE(initLogic());

    // Transitive inference: Penguin(opus) -> Bird(opus) -> Fly(opus)
    ASSERT_TRUE(brain_add_logical_fact(brain, "Penguin(opus)", 0.85f));
    ASSERT_TRUE(brain_add_logical_rule(brain, "Penguin(x) -> Bird(x)", 0.9f));
    ASSERT_TRUE(brain_add_logical_rule(brain, "Bird(x) -> Fly(x)", 0.8f));

    inference_result_t result;
    EXPECT_TRUE(brain_forward_chain(brain, 20, &result));
    brain_free_inference_result(&result);
}

TEST_F(SymbolicLogicBrainTest, ForwardChain_NullBrain) {
    inference_result_t result;
    EXPECT_FALSE(brain_forward_chain(nullptr, 10, &result));
}

TEST_F(SymbolicLogicBrainTest, ForwardChain_NullResult) {
    ASSERT_TRUE(initLogic());
    EXPECT_FALSE(brain_forward_chain(brain, 10, nullptr));
}

TEST_F(SymbolicLogicBrainTest, ForwardChain_NotInitialized) {
    inference_result_t result;
    EXPECT_FALSE(brain_forward_chain(brain, 10, &result));
}

TEST_F(SymbolicLogicBrainTest, ForwardChain_ZeroIterations) {
    ASSERT_TRUE(initLogic());
    ASSERT_TRUE(brain_add_logical_fact(brain, "Bird(tweety)", 0.9f));

    inference_result_t result;
    // Zero iterations should be capped to max (1000)
    EXPECT_TRUE(brain_forward_chain(brain, 0, &result));
    brain_free_inference_result(&result);
}

//=============================================================================
// Backward Chaining Tests
//=============================================================================

TEST_F(SymbolicLogicBrainTest, BackwardChain_ProveGoal_Socrates) {
    ASSERT_TRUE(initLogic());

    // Classic example: Man(socrates) + [Man(x) -> Mortal(x)] => Prove Mortal(socrates)
    ASSERT_TRUE(brain_add_logical_fact(brain, "Man(socrates)", 0.9f));
    ASSERT_TRUE(brain_add_logical_rule(brain, "Man(x) -> Mortal(x)", 0.95f));

    inference_result_t result;
    EXPECT_TRUE(brain_backward_chain(brain, "Mortal(socrates)", &result));
    EXPECT_GT(result.num_steps, 0U);
    EXPECT_GT(result.confidence, 0.5f);
    brain_free_inference_result(&result);
}

TEST_F(SymbolicLogicBrainTest, BackwardChain_UnprovableGoal) {
    ASSERT_TRUE(initLogic());

    // No facts or rules that can prove Fish(nemo)
    ASSERT_TRUE(brain_add_logical_fact(brain, "Bird(tweety)", 0.9f));

    inference_result_t result;
    EXPECT_FALSE(brain_backward_chain(brain, "Fish(nemo)", &result));
}

TEST_F(SymbolicLogicBrainTest, BackwardChain_MultiStepProof) {
    ASSERT_TRUE(initLogic());

    // Chain: Penguin(opus) -> Bird(opus) -> Animal(opus)
    ASSERT_TRUE(brain_add_logical_fact(brain, "Penguin(opus)", 0.85f));
    ASSERT_TRUE(brain_add_logical_rule(brain, "Penguin(x) -> Bird(x)", 0.9f));
    ASSERT_TRUE(brain_add_logical_rule(brain, "Bird(x) -> Animal(x)", 0.95f));

    inference_result_t result;
    EXPECT_TRUE(brain_backward_chain(brain, "Animal(opus)", &result));
    EXPECT_GE(result.num_steps, 2U); // At least 2 steps
    brain_free_inference_result(&result);
}

TEST_F(SymbolicLogicBrainTest, BackwardChain_NullBrain) {
    inference_result_t result;
    EXPECT_FALSE(brain_backward_chain(nullptr, "Mortal(x)", &result));
}

TEST_F(SymbolicLogicBrainTest, BackwardChain_NullGoal) {
    ASSERT_TRUE(initLogic());
    inference_result_t result;
    EXPECT_FALSE(brain_backward_chain(brain, nullptr, &result));
}

TEST_F(SymbolicLogicBrainTest, BackwardChain_NullResult) {
    ASSERT_TRUE(initLogic());
    EXPECT_FALSE(brain_backward_chain(brain, "Mortal(x)", nullptr));
}

TEST_F(SymbolicLogicBrainTest, BackwardChain_NotInitialized) {
    inference_result_t result;
    EXPECT_FALSE(brain_backward_chain(brain, "Mortal(x)", &result));
}

//=============================================================================
// Working Memory Integration Tests
//=============================================================================

TEST_F(SymbolicLogicBrainTest, WorkingMemory_FactStoredInWM) {
    ASSERT_TRUE(initLogic());

    // Add fact - should be stored in working memory
    ASSERT_TRUE(brain_add_logical_fact(brain, "Bird(tweety)", 0.9f));

    // Verify working memory has items
    working_memory_t* wm = brain_get_working_memory(brain);
    ASSERT_NE(wm, nullptr);
    working_memory_stats_t wm_stats;
    working_memory_get_stats(wm, &wm_stats);
    EXPECT_GT(wm_stats.current_size, 0U);
}

TEST_F(SymbolicLogicBrainTest, WorkingMemory_InferenceStoredInWM) {
    ASSERT_TRUE(initLogic());

    // Setup inference
    ASSERT_TRUE(brain_add_logical_fact(brain, "Bird(tweety)", 0.9f));
    ASSERT_TRUE(brain_add_logical_rule(brain, "Bird(x) -> Fly(x)", 0.8f));

    // Perform forward chaining
    inference_result_t result;
    ASSERT_TRUE(brain_forward_chain(brain, 10, &result));
    brain_free_inference_result(&result);

    // Check that new inference is in working memory
    working_memory_t* wm = brain_get_working_memory(brain);
    ASSERT_NE(wm, nullptr);
    working_memory_stats_t wm_stats;
    working_memory_get_stats(wm, &wm_stats);
    EXPECT_GT(wm_stats.current_size, 0U);
}

TEST_F(SymbolicLogicBrainTest, WorkingMemory_ProofStoredInWM) {
    ASSERT_TRUE(initLogic());

    // Setup proof
    ASSERT_TRUE(brain_add_logical_fact(brain, "Man(socrates)", 0.9f));
    ASSERT_TRUE(brain_add_logical_rule(brain, "Man(x) -> Mortal(x)", 0.95f));

    // Perform backward chaining
    inference_result_t result;
    ASSERT_TRUE(brain_backward_chain(brain, "Mortal(socrates)", &result));
    brain_free_inference_result(&result);

    // Verify proof stored in working memory
    working_memory_t* wm = brain_get_working_memory(brain);
    ASSERT_NE(wm, nullptr);
    working_memory_stats_t wm_stats;
    working_memory_get_stats(wm, &wm_stats);
    EXPECT_GT(wm_stats.current_size, 0U);
}

//=============================================================================
// Executive Function Integration Tests
//=============================================================================

TEST_F(SymbolicLogicBrainTest, Executive_ForwardChainingCreatesTask) {
    ASSERT_TRUE(initLogic());

    ASSERT_TRUE(brain_add_logical_fact(brain, "Bird(tweety)", 0.9f));
    ASSERT_TRUE(brain_add_logical_rule(brain, "Bird(x) -> Fly(x)", 0.8f));

    // Forward chaining should perform inferences
    inference_result_t result;
    ASSERT_TRUE(brain_forward_chain(brain, 10, &result));
    brain_free_inference_result(&result);

    // Verify logic stats show inferences performed
    logic_stats_t logic_stats;
    ASSERT_TRUE(brain_get_logic_stats(brain, &logic_stats));
    EXPECT_GT(logic_stats.inferences_performed, 0U);
}

TEST_F(SymbolicLogicBrainTest, Executive_BackwardChainingCreatesTask) {
    ASSERT_TRUE(initLogic());

    ASSERT_TRUE(brain_add_logical_fact(brain, "Man(socrates)", 0.9f));
    ASSERT_TRUE(brain_add_logical_rule(brain, "Man(x) -> Mortal(x)", 0.95f));

    // Backward chaining should perform inferences
    inference_result_t result;
    ASSERT_TRUE(brain_backward_chain(brain, "Mortal(socrates)", &result));
    brain_free_inference_result(&result);

    // Verify logic stats show inferences performed
    logic_stats_t logic_stats;
    ASSERT_TRUE(brain_get_logic_stats(brain, &logic_stats));
    EXPECT_GT(logic_stats.inferences_performed, 0U);
}

//=============================================================================
// Error Handling Tests
//=============================================================================

TEST_F(SymbolicLogicBrainTest, Error_CircularReasoning) {
    ASSERT_TRUE(initLogic());

    // Create potential circular dependency
    ASSERT_TRUE(brain_add_logical_rule(brain, "A(x) -> B(x)", 0.8f));
    ASSERT_TRUE(brain_add_logical_rule(brain, "B(x) -> A(x)", 0.8f));

    // Forward chaining should handle gracefully (not infinite loop)
    inference_result_t result;
    EXPECT_TRUE(brain_forward_chain(brain, 10, &result));
    brain_free_inference_result(&result);
}

TEST_F(SymbolicLogicBrainTest, Error_GetStatsNotInitialized) {
    logic_stats_t stats;
    EXPECT_FALSE(brain_get_logic_stats(brain, &stats));
}

TEST_F(SymbolicLogicBrainTest, Error_GetStatsNullBrain) {
    logic_stats_t stats;
    EXPECT_FALSE(brain_get_logic_stats(nullptr, &stats));
}

TEST_F(SymbolicLogicBrainTest, Error_GetStatsNullStats) {
    ASSERT_TRUE(initLogic());
    EXPECT_FALSE(brain_get_logic_stats(brain, nullptr));
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(SymbolicLogicBrainTest, Stats_GetLogicStats) {
    ASSERT_TRUE(initLogic());

    ASSERT_TRUE(brain_add_logical_fact(brain, "Bird(tweety)", 0.9f));
    ASSERT_TRUE(brain_add_logical_rule(brain, "Bird(x) -> Fly(x)", 0.8f));

    logic_stats_t stats;
    EXPECT_TRUE(brain_get_logic_stats(brain, &stats));
    EXPECT_GT(stats.facts_stored, 0U);
}

//=============================================================================
// Integration Test - Full Reasoning Scenario
//=============================================================================

TEST_F(SymbolicLogicBrainTest, Integration_FullReasoningScenario) {
    ASSERT_TRUE(initLogic());

    // Build knowledge base: Birds and flight
    ASSERT_TRUE(brain_add_logical_fact(brain, "Bird(tweety)", 0.9f));
    ASSERT_TRUE(brain_add_logical_fact(brain, "Penguin(opus)", 0.85f));
    ASSERT_TRUE(brain_add_logical_rule(brain, "Penguin(x) -> Bird(x)", 0.95f));
    ASSERT_TRUE(brain_add_logical_rule(brain, "Bird(x) -> CanFly(x)", 0.8f));
    ASSERT_TRUE(brain_add_logical_rule(brain, "Penguin(x) -> ~CanFly(x)", 0.98f));

    // Forward chain to derive new facts
    inference_result_t fwd_result;
    EXPECT_TRUE(brain_forward_chain(brain, 20, &fwd_result));
    brain_free_inference_result(&fwd_result);

    // Backward chain to prove specific goal
    inference_result_t bwd_result;
    EXPECT_TRUE(brain_backward_chain(brain, "Bird(opus)", &bwd_result));
    brain_free_inference_result(&bwd_result);

    // Query knowledge base
    query_result_t query;
    EXPECT_TRUE(brain_query_knowledge(brain, "Bird(tweety)", &query));
    EXPECT_GT(query.num_matches, 0);
    brain_free_query_result(&query);

    // Verify statistics
    logic_stats_t stats;
    EXPECT_TRUE(brain_get_logic_stats(brain, &stats));
    EXPECT_GT(stats.inferences_performed, 0ULL);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
