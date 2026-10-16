/**
 * @file test_symbolic_logic_reasoning_regression.cpp
 * @brief Regression tests for symbolic logic in the reasoning chain
 *
 * WHAT: Guards against edge-case bugs in symbolic logic reasoning phases
 * WHY:  Prevent regressions in NULL handling, empty KB, extreme config values
 * HOW:  Each test targets one specific edge case
 *
 * @author NIMCP Development Team
 * @date 2026-02-25
 * @version 1.0.0
 */

#include <gtest/gtest.h>
#include <cstring>
#include <string>
#include <climits>

extern "C" {
#include "core/brain/nimcp_brain.h"
#include "cognitive/reasoning/nimcp_reasoning_chain.h"
#include "cognitive/reasoning/nimcp_symbolic_logic_brain_integration.h"
#include "cognitive/reasoning/nimcp_symbolic_logic_attachment.h"
}

// =============================================================================
// Constants
// =============================================================================

static constexpr uint32_t INPUT_DIM = 32;
static constexpr uint32_t OUTPUT_DIM = 8;

// =============================================================================
// Test Fixture
// =============================================================================

class SymbolicLogicReasoningRegression : public ::testing::Test {
protected:
    brain_t brain = nullptr;
    reasoning_engine_t* engine = nullptr;

    void SetUp() override {
        // Create brain
        brain = brain_create("symlog_regression", BRAIN_SIZE_SMALL,
                            BRAIN_TASK_CLASSIFICATION, INPUT_DIM, OUTPUT_DIM);
        ASSERT_NE(brain, nullptr) << "Brain creation failed";

        // Create reasoning engine with symbolic logic enabled
        reasoning_engine_config_t cfg = reasoning_engine_default_config();
        cfg.enable_symbolic_logic = true;
        cfg.symbolic_inference_depth = 10;
        engine = reasoning_engine_create(&cfg);
        ASSERT_NE(engine, nullptr) << "Reasoning engine creation failed";
    }

    void TearDown() override {
        if (engine) {
            reasoning_engine_destroy(engine);
            engine = nullptr;
        }
        if (brain) {
            brain_destroy(brain);
            brain = nullptr;
        }
    }
};

// =============================================================================
// Regression: Symbolic Logic Edge Cases
// =============================================================================

TEST_F(SymbolicLogicReasoningRegression, NullBrainSymbolicLogicDoesNotCrash) {
    // Regression: Engine with symbolic logic enabled but no brain connected
    // must return error, not segfault. The symbolic logic phase must handle
    // the absence of a brain gracefully.
    reasoning_chain_t chain;
    reasoning_chain_init(&chain);

    // Do NOT connect brain — engine has no brain attached
    int rc = reasoning_engine_reason(engine, "What is mortal?", &chain);

    // Should return error (not crash) because no brain is connected
    EXPECT_NE(rc, 0) << "Reasoning without connected brain should return error";

    reasoning_chain_cleanup(&chain);
}

TEST_F(SymbolicLogicReasoningRegression, DisabledSymbolicLogicStillReasons) {
    // Regression: When symbolic logic is explicitly disabled, the reasoning
    // pipeline should still complete successfully using other phases.
    // The chain should contain no SYMBOLIC_LOGIC steps.
    reasoning_engine_t* no_symlog_engine = nullptr;

    reasoning_engine_config_t cfg = reasoning_engine_default_config();
    cfg.enable_symbolic_logic = false;
    no_symlog_engine = reasoning_engine_create(&cfg);
    ASSERT_NE(no_symlog_engine, nullptr);

    int rc = reasoning_engine_connect_brain(no_symlog_engine, brain);
    ASSERT_EQ(rc, 0) << "Brain connection failed";

    reasoning_chain_t chain;
    reasoning_chain_init(&chain);

    rc = reasoning_engine_reason(no_symlog_engine, "What are neurons?", &chain);
    EXPECT_EQ(rc, 0) << "Reasoning with symbolic logic disabled should succeed";

    // Verify no SYMBOLIC_LOGIC steps appear in the chain
    uint32_t num_steps = reasoning_chain_get_num_steps(&chain);
    for (uint32_t i = 0; i < num_steps; i++) {
        const reasoning_step_t* step = reasoning_chain_get_step(&chain, i);
        if (step) {
            EXPECT_NE(step->type, REASONING_STEP_SYMBOLIC_LOGIC)
                << "Step " << i << " should not be SYMBOLIC_LOGIC when disabled";
        }
    }

    // Confidence should still be valid
    float conf = reasoning_chain_get_confidence(&chain);
    EXPECT_GE(conf, 0.0f);
    EXPECT_LE(conf, 1.0f);

    reasoning_chain_cleanup(&chain);
    reasoning_engine_destroy(no_symlog_engine);
}

TEST_F(SymbolicLogicReasoningRegression, SymbolicInferenceDepthZeroHandled) {
    // Regression: Setting symbolic_inference_depth to 0 should not crash.
    // Engine creation should succeed, and reasoning should not attempt
    // unbounded backward chaining.
    reasoning_engine_t* zero_depth_engine = nullptr;

    reasoning_engine_config_t cfg = reasoning_engine_default_config();
    cfg.enable_symbolic_logic = true;
    cfg.symbolic_inference_depth = 0;
    zero_depth_engine = reasoning_engine_create(&cfg);
    ASSERT_NE(zero_depth_engine, nullptr)
        << "Engine creation with depth=0 should succeed";

    int rc = reasoning_engine_connect_brain(zero_depth_engine, brain);
    ASSERT_EQ(rc, 0) << "Brain connection failed";

    // Initialize symbolic logic on the brain so the phase has something to work with
    logic_brain_config_t lcfg = {};
    lcfg.max_facts = 100;
    lcfg.max_rules = 50;
    lcfg.max_inference_depth = 0;
    lcfg.enable_forward_chaining = true;
    lcfg.enable_backward_chaining = true;
    lcfg.enable_wm_integration = false;
    lcfg.enable_exec_integration = false;
    lcfg.wm_inference_salience = 0.5f;
    brain_create_symbolic_logic(brain, &lcfg);

    reasoning_chain_t chain;
    reasoning_chain_init(&chain);

    rc = reasoning_engine_reason(zero_depth_engine, "Is socrates mortal?", &chain);
    // Should either succeed or fail gracefully — no crash
    if (rc == 0) {
        EXPECT_GE(chain.num_steps, 0u);
        float conf = reasoning_chain_get_confidence(&chain);
        EXPECT_GE(conf, 0.0f);
        EXPECT_LE(conf, 1.0f);
    }

    reasoning_chain_cleanup(&chain);
    brain_destroy_symbolic_logic(brain);
    reasoning_engine_destroy(zero_depth_engine);
}

TEST_F(SymbolicLogicReasoningRegression, SymbolicInferenceDepthMaxHandled) {
    // Regression: Setting symbolic_inference_depth to UINT32_MAX should not
    // cause integer overflow, stack overflow, or infinite loops.
    // The engine should clamp or handle gracefully.
    reasoning_engine_t* max_depth_engine = nullptr;

    reasoning_engine_config_t cfg = reasoning_engine_default_config();
    cfg.enable_symbolic_logic = true;
    cfg.symbolic_inference_depth = UINT32_MAX;
    max_depth_engine = reasoning_engine_create(&cfg);
    ASSERT_NE(max_depth_engine, nullptr)
        << "Engine creation with depth=UINT32_MAX should succeed";

    int rc = reasoning_engine_connect_brain(max_depth_engine, brain);
    ASSERT_EQ(rc, 0) << "Brain connection failed";

    // Initialize symbolic logic on the brain
    logic_brain_config_t lcfg = {};
    lcfg.max_facts = 100;
    lcfg.max_rules = 50;
    lcfg.max_inference_depth = 10;  // Brain-level depth is separate
    lcfg.enable_forward_chaining = true;
    lcfg.enable_backward_chaining = true;
    lcfg.enable_wm_integration = false;
    lcfg.enable_exec_integration = false;
    lcfg.wm_inference_salience = 0.5f;
    brain_create_symbolic_logic(brain, &lcfg);

    // Add a simple fact so the engine has something to work with
    brain_add_logical_fact(brain, "Bird(tweety)", 0.9f);

    reasoning_chain_t chain;
    reasoning_chain_init(&chain);

    rc = reasoning_engine_reason(max_depth_engine, "Can tweety fly?", &chain);
    // Should either succeed or fail gracefully — no crash, no infinite loop
    if (rc == 0) {
        EXPECT_GE(chain.num_steps, 0u);
        float conf = reasoning_chain_get_confidence(&chain);
        EXPECT_GE(conf, 0.0f);
        EXPECT_LE(conf, 1.0f);
    }

    reasoning_chain_cleanup(&chain);
    brain_destroy_symbolic_logic(brain);
    reasoning_engine_destroy(max_depth_engine);
}

TEST_F(SymbolicLogicReasoningRegression, VeryLongQueryWithSymbolicLogic) {
    // Regression: A 10000-char query with symbolic logic enabled must not
    // cause buffer overflow in the symbolic logic phase. The query gets
    // passed through hashing and feature extraction before reaching
    // symbolic phases, but we must ensure no intermediate buffer overflows.
    int rc = reasoning_engine_connect_brain(engine, brain);
    ASSERT_EQ(rc, 0) << "Brain connection failed";

    // Initialize symbolic logic with some content
    logic_brain_config_t lcfg = {};
    lcfg.max_facts = 100;
    lcfg.max_rules = 50;
    lcfg.max_inference_depth = 5;
    lcfg.enable_forward_chaining = true;
    lcfg.enable_backward_chaining = true;
    lcfg.enable_wm_integration = false;
    lcfg.enable_exec_integration = false;
    lcfg.wm_inference_salience = 0.5f;
    brain_create_symbolic_logic(brain, &lcfg);

    brain_add_logical_fact(brain, "Bird(tweety)", 0.9f);
    brain_add_logical_rule(brain, "Bird(x) -> Fly(x)", 0.8f);

    // Build a 10000-char query with variety to prevent single-char optimization
    std::string long_query(10000, 'A');
    for (size_t i = 0; i < long_query.size(); i += 100) {
        long_query[i] = 'a' + static_cast<char>(i % 26);
    }

    reasoning_chain_t chain;
    reasoning_chain_init(&chain);

    rc = reasoning_engine_reason(engine, long_query.c_str(), &chain);
    // Should either succeed or fail gracefully — no crash, no overflow
    if (rc == 0) {
        EXPECT_GE(chain.num_steps, 0u);
    }

    reasoning_chain_cleanup(&chain);
    brain_destroy_symbolic_logic(brain);
}

TEST_F(SymbolicLogicReasoningRegression, EmptyKBReasoningGraceful) {
    // Regression: Connect brain with symbolic logic engine but add no facts
    // or rules. The reasoning phase should handle the empty KB gracefully
    // without crashing or returning garbage data.
    int rc = reasoning_engine_connect_brain(engine, brain);
    ASSERT_EQ(rc, 0) << "Brain connection failed";

    // Initialize symbolic logic with EMPTY knowledge base
    logic_brain_config_t lcfg = {};
    lcfg.max_facts = 100;
    lcfg.max_rules = 50;
    lcfg.max_inference_depth = 5;
    lcfg.enable_forward_chaining = true;
    lcfg.enable_backward_chaining = true;
    lcfg.enable_wm_integration = false;
    lcfg.enable_exec_integration = false;
    lcfg.wm_inference_salience = 0.5f;
    bool created = brain_create_symbolic_logic(brain, &lcfg);
    ASSERT_TRUE(created) << "Symbolic logic creation failed";

    // Deliberately do NOT add any facts or rules

    reasoning_chain_t chain;
    reasoning_chain_init(&chain);

    rc = reasoning_engine_reason(engine, "Is socrates mortal?", &chain);
    // Reasoning should succeed (other phases still contribute)
    // or fail gracefully — the key assertion is no crash
    if (rc == 0) {
        EXPECT_GE(chain.num_steps, 0u);
        float conf = reasoning_chain_get_confidence(&chain);
        EXPECT_GE(conf, 0.0f);
        EXPECT_LE(conf, 1.0f);

        // If the chain has a conclusion, it should be a valid string
        if (chain.is_complete) {
            size_t conclusion_len = strlen(chain.conclusion);
            EXPECT_LT(conclusion_len, (size_t)REASONING_CHAIN_CONCLUSION_LEN)
                << "Conclusion should fit within buffer";
        }
    }

    reasoning_chain_cleanup(&chain);
    brain_destroy_symbolic_logic(brain);
}
