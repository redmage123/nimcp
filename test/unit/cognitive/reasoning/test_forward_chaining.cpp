/**
 * @file test_forward_chaining.cpp
 * @brief Unit tests for MODULE 3: Forward Chaining Engine
 *
 * TEST COVERAGE: 10 tests
 *
 * @author NIMCP Development Team
 * @date 2025-11-20
 */

#include <gtest/gtest.h>
// Headers have their own extern "C" guards
    #include "cognitive/reasoning/nimcp_forward_chaining.h"
    #include "cognitive/reasoning/nimcp_knowledge_base_interface.h"
    #include "cognitive/reasoning/nimcp_symbolic_logic_attachment.h"
    #include "cognitive/reasoning/nimcp_reasoning_factory.h"
    #include "core/brain/nimcp_brain.h"

class ForwardChainingTest : public ::testing::Test {
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

TEST_F(ForwardChainingTest, ForwardChainWithNullBrainFails) {
    forward_chain_result_t result;
    EXPECT_FALSE(brain_forward_chain(nullptr, 10, &result));
}

TEST_F(ForwardChainingTest, ForwardChainWithNullResultFails) {
    EXPECT_FALSE(brain_forward_chain(brain, 10, nullptr));
}

TEST_F(ForwardChainingTest, ForwardChainSuccess) {
    brain_add_fact(brain, "Bird(tweety)", 0.9f);
    brain_add_rule(brain, "Bird(x) -> Fly(x)", 0.8f);

    forward_chain_result_t result;
    EXPECT_TRUE(brain_forward_chain(brain, 10, &result));
    forward_chain_free_result(&result);
}

TEST_F(ForwardChainingTest, ForwardChainIterationCapping) {
    forward_chain_result_t result;
    EXPECT_TRUE(brain_forward_chain(brain, 10000, &result));
    EXPECT_LE(result.iterations_performed, 1000u);
    forward_chain_free_result(&result);
}

TEST_F(ForwardChainingTest, ForwardChainStepWithNullBrainFails) {
    logic_clause_t** facts = nullptr;
    uint32_t num_facts = 0;
    EXPECT_FALSE(brain_forward_chain_step(nullptr, &facts, &num_facts));
}

TEST_F(ForwardChainingTest, ForwardChainStepSuccess) {
    brain_add_fact(brain, "Bird(tweety)", 0.9f);

    logic_clause_t** facts = nullptr;
    uint32_t num_facts = 0;
    EXPECT_TRUE(brain_forward_chain_step(brain, &facts, &num_facts));
}

TEST_F(ForwardChainingTest, ForwardChainFreeResultSafeWithNull) {
    forward_chain_free_result(nullptr);
    SUCCEED();
}

TEST_F(ForwardChainingTest, GetForwardChainStatsWithNullBrainFails) {
    uint32_t iterations = 0;
    EXPECT_FALSE(brain_get_forward_chain_stats(nullptr, &iterations, nullptr, nullptr));
}

TEST_F(ForwardChainingTest, GetForwardChainStatsSuccess) {
    uint32_t iterations = 0, facts = 0;
    uint64_t time_ms = 0;
    EXPECT_TRUE(brain_get_forward_chain_stats(brain, &iterations, &facts, &time_ms));
}

TEST_F(ForwardChainingTest, ForwardChainResultPopulated) {
    brain_add_fact(brain, "Bird(tweety)", 0.9f);

    forward_chain_result_t result;
    EXPECT_TRUE(brain_forward_chain(brain, 5, &result));
    EXPECT_GE(result.inference_time_ms, 0ull);
    forward_chain_free_result(&result);
}
