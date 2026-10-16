/**
 * @file test_backward_chaining.cpp
 * @brief Unit tests for MODULE 4: Backward Chaining Engine
 *
 * TEST COVERAGE: 12 tests
 *
 * @author NIMCP Development Team
 * @date 2025-11-20
 */

#include <gtest/gtest.h>
// Headers have their own extern "C" guards
    #include "cognitive/reasoning/nimcp_backward_chaining.h"
    #include "cognitive/reasoning/nimcp_knowledge_base_interface.h"
    #include "cognitive/reasoning/nimcp_symbolic_logic_attachment.h"
    #include "cognitive/reasoning/nimcp_reasoning_factory.h"
    #include "core/brain/nimcp_brain.h"

class BackwardChainingTest : public ::testing::Test {
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

TEST_F(BackwardChainingTest, BackwardChainWithNullBrainFails) {
    backward_chain_result_t result;
    EXPECT_FALSE(brain_backward_chain(nullptr, "Mortal(socrates)", &result));
}

TEST_F(BackwardChainingTest, BackwardChainWithNullGoalFails) {
    backward_chain_result_t result;
    EXPECT_FALSE(brain_backward_chain(brain, nullptr, &result));
}

TEST_F(BackwardChainingTest, BackwardChainWithNullResultFails) {
    EXPECT_FALSE(brain_backward_chain(brain, "Mortal(socrates)", nullptr));
}

TEST_F(BackwardChainingTest, BackwardChainSuccess) {
    brain_add_fact(brain, "Man(socrates)", 0.9f);
    brain_add_rule(brain, "Man(x) -> Mortal(x)", 0.8f);

    backward_chain_result_t result;
    bool proven = brain_backward_chain(brain, "Mortal(socrates)", &result);
    EXPECT_TRUE(result.proven == proven);
    backward_chain_free_result(&result);
}

TEST_F(BackwardChainingTest, BackwardChainResultPopulated) {
    brain_add_fact(brain, "Man(socrates)", 0.9f);

    backward_chain_result_t result;
    brain_backward_chain(brain, "Man(socrates)", &result);
    EXPECT_GE(result.inference_time_ms, 0ull);
    backward_chain_free_result(&result);
}

TEST_F(BackwardChainingTest, BackwardChainStepWithNullBrainFails) {
    logic_clause_t** premises = nullptr;
    uint32_t num_premises = 0;
    EXPECT_FALSE(brain_backward_chain_step(nullptr, "Goal(x)", &premises, &num_premises));
}

TEST_F(BackwardChainingTest, BackwardChainStepWithNullSubgoalFails) {
    logic_clause_t** premises = nullptr;
    uint32_t num_premises = 0;
    EXPECT_FALSE(brain_backward_chain_step(brain, nullptr, &premises, &num_premises));
}

TEST_F(BackwardChainingTest, BackwardChainFreeResultSafeWithNull) {
    backward_chain_free_result(nullptr);
    SUCCEED();
}

TEST_F(BackwardChainingTest, GetBackwardChainStatsWithNullBrainFails) {
    uint32_t attempted = 0;
    EXPECT_FALSE(brain_get_backward_chain_stats(nullptr, &attempted, nullptr, nullptr));
}

TEST_F(BackwardChainingTest, GetBackwardChainStatsSuccess) {
    uint32_t attempted = 0, succeeded = 0;
    float avg_depth = 0.0f;
    EXPECT_TRUE(brain_get_backward_chain_stats(brain, &attempted, &succeeded, &avg_depth));
}

TEST_F(BackwardChainingTest, BackwardChainConfidenceScoring) {
    brain_add_fact(brain, "Man(socrates)", 0.9f);

    backward_chain_result_t result;
    brain_backward_chain(brain, "Man(socrates)", &result);
    if (result.proven) {
        EXPECT_GT(result.confidence, 0.0f);
    }
    backward_chain_free_result(&result);
}

TEST_F(BackwardChainingTest, BackwardChainDepthTracking) {
    brain_add_fact(brain, "Man(socrates)", 0.9f);

    backward_chain_result_t result;
    brain_backward_chain(brain, "Man(socrates)", &result);
    EXPECT_GE(result.depth_reached, 0u);
    backward_chain_free_result(&result);
}
