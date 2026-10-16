/**
 * @file test_symbolic_logic_attachment.cpp
 * @brief Unit tests for MODULE 1: Symbolic Logic Attachment
 *
 * TEST COVERAGE: 8 tests
 * - Attach logic engine to brain (2 tests)
 * - Detach logic engine from brain (2 tests)
 * - Get logic engine from brain (2 tests)
 * - Check if brain has logic engine (2 tests)
 *
 * @author NIMCP Development Team
 * @date 2025-11-20
 */

#include <gtest/gtest.h>
// Headers have their own extern "C" guards
    #include "cognitive/reasoning/nimcp_symbolic_logic_attachment.h"
    #include "cognitive/reasoning/nimcp_reasoning_factory.h"
    #include "core/brain/nimcp_brain.h"

class SymbolicLogicAttachmentTest : public ::testing::Test {
protected:
    brain_t brain;
    symbolic_logic_t* engine;

    void SetUp() override {
        brain = brain_create("test_brain", BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION, 10, 5);
        ASSERT_NE(brain, nullptr);

        engine = create_default_symbolic_logic(REASONING_SIZE_SMALL);
        ASSERT_NE(engine, nullptr);
    }

    void TearDown() override {
        if (brain) {
            symbolic_logic_t* attached = brain_get_symbolic_logic(brain);
            if (attached) {
                brain_detach_symbolic_logic(brain);
                symbolic_logic_destroy(attached);
            }
            brain_destroy(brain);
        }
    }
};

// Test 1: Attach logic engine successfully
TEST_F(SymbolicLogicAttachmentTest, AttachLogicEngineSuccess) {
    ASSERT_TRUE(brain_attach_symbolic_logic(brain, engine));
    EXPECT_TRUE(brain_has_symbolic_logic(brain));
    EXPECT_EQ(brain_get_symbolic_logic(brain), engine);
}

// Test 2: Attach logic engine with NULL brain fails
TEST_F(SymbolicLogicAttachmentTest, AttachLogicEngineNullBrainFails) {
    ASSERT_FALSE(brain_attach_symbolic_logic(nullptr, engine));
}

// Test 3: Detach logic engine successfully
TEST_F(SymbolicLogicAttachmentTest, DetachLogicEngineSuccess) {
    brain_attach_symbolic_logic(brain, engine);
    symbolic_logic_t* detached = brain_detach_symbolic_logic(brain);
    EXPECT_EQ(detached, engine);
    EXPECT_FALSE(brain_has_symbolic_logic(brain));
    symbolic_logic_destroy(detached);
}

// Test 4: Detach from brain with no engine attached
TEST_F(SymbolicLogicAttachmentTest, DetachNothingAttached) {
    symbolic_logic_t* detached = brain_detach_symbolic_logic(brain);
    EXPECT_EQ(detached, nullptr);
}

// Test 5: Get logic engine when attached
TEST_F(SymbolicLogicAttachmentTest, GetLogicEngineWhenAttached) {
    brain_attach_symbolic_logic(brain, engine);
    EXPECT_EQ(brain_get_symbolic_logic(brain), engine);
}

// Test 6: Get logic engine when not attached
TEST_F(SymbolicLogicAttachmentTest, GetLogicEngineWhenNotAttached) {
    EXPECT_EQ(brain_get_symbolic_logic(brain), nullptr);
}

// Test 7: Has logic engine returns true when attached
TEST_F(SymbolicLogicAttachmentTest, HasLogicEngineTrue) {
    brain_attach_symbolic_logic(brain, engine);
    EXPECT_TRUE(brain_has_symbolic_logic(brain));
}

// Test 8: Has logic engine returns false when not attached
TEST_F(SymbolicLogicAttachmentTest, HasLogicEngineFalse) {
    EXPECT_FALSE(brain_has_symbolic_logic(brain));
}
