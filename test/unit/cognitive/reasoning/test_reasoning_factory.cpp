/**
 * @file test_reasoning_factory.cpp
 * @brief Unit tests for MODULE 6: Reasoning Factory
 *
 * TEST COVERAGE: 6 tests
 *
 * @author NIMCP Development Team
 * @date 2025-11-20
 */

#include <gtest/gtest.h>
// Headers have their own extern "C" guards
    #include "cognitive/reasoning/nimcp_reasoning_factory.h"

class ReasoningFactoryTest : public ::testing::Test {
protected:
    void TearDown() override {
        // Clean up any created engines
    }
};

TEST_F(ReasoningFactoryTest, CreateDefaultSmallEngine) {
    symbolic_logic_t* engine = create_default_symbolic_logic(REASONING_SIZE_SMALL);
    EXPECT_NE(engine, nullptr);
    if (engine) symbolic_logic_destroy(engine);
}

TEST_F(ReasoningFactoryTest, CreateDefaultMediumEngine) {
    symbolic_logic_t* engine = create_default_symbolic_logic(REASONING_SIZE_MEDIUM);
    EXPECT_NE(engine, nullptr);
    if (engine) symbolic_logic_destroy(engine);
}

TEST_F(ReasoningFactoryTest, CreateDefaultLargeEngine) {
    symbolic_logic_t* engine = create_default_symbolic_logic(REASONING_SIZE_LARGE);
    EXPECT_NE(engine, nullptr);
    if (engine) symbolic_logic_destroy(engine);
}

TEST_F(ReasoningFactoryTest, CreateForwardChainingEngine) {
    symbolic_logic_t* engine = create_forward_chaining_engine(REASONING_SIZE_MEDIUM);
    EXPECT_NE(engine, nullptr);
    if (engine) symbolic_logic_destroy(engine);
}

TEST_F(ReasoningFactoryTest, CreateBackwardChainingEngine) {
    symbolic_logic_t* engine = create_backward_chaining_engine(REASONING_SIZE_MEDIUM);
    EXPECT_NE(engine, nullptr);
    if (engine) symbolic_logic_destroy(engine);
}

TEST_F(ReasoningFactoryTest, CreateWithCustomConfig) {
    logic_config_t config = {
        .max_predicates = 100,
        .max_rules = 50,
        .max_kb_size = 100,
        .max_inference_depth = 5,
        .enable_forward_chaining = true,
        .enable_backward_chaining = true,
        .enable_resolution = true,
        .enable_memory_consolidation = false
    };

    symbolic_logic_t* engine = create_symbolic_logic_with_config(&config);
    EXPECT_NE(engine, nullptr);
    if (engine) symbolic_logic_destroy(engine);
}
