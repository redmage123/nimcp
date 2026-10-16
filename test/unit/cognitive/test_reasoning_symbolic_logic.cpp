/**
 * @file test_reasoning_symbolic_logic.cpp
 * @brief Unit tests for symbolic logic integration in the reasoning chain
 *
 * WHAT: Tests the REASONING_STEP_SYMBOLIC_LOGIC step type, config fields,
 *       and stats fields added for symbolic logic integration
 * WHY:  Verify symbolic logic chain operations work correctly in isolation
 *       (no brain dependency — pure data structure + config correctness)
 * HOW:  GTest fixture exercising chain API with symbolic logic step types
 *
 * @author NIMCP Development Team
 * @date 2026-02-25
 * @version 1.0.0
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <chrono>

extern "C" {
#include "cognitive/reasoning/nimcp_reasoning_chain.h"
}

// =============================================================================
// Test Fixture
// =============================================================================

class ReasoningSymbolicLogicUnit : public ::testing::Test {
protected:
    reasoning_engine_t* engine = nullptr;
    reasoning_chain_t chain;

    void SetUp() override {
        memset(&chain, 0, sizeof(chain));
    }

    void TearDown() override {
        if (engine) {
            reasoning_engine_destroy(engine);
            engine = nullptr;
        }
        reasoning_chain_cleanup(&chain);
    }

    // Helper: create a step with given type and confidence
    reasoning_step_t make_step(reasoning_step_type_t type, float confidence,
                               float relevance, const char* desc) {
        reasoning_step_t step;
        memset(&step, 0, sizeof(step));
        step.step_id = 0; // Will be assigned by chain
        step.type = type;
        step.confidence = confidence;
        step.relevance = relevance;
        step.timestamp_us = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now().time_since_epoch()
            ).count()
        );
        if (desc) {
            strncpy(step.description, desc, sizeof(step.description) - 1);
            step.description[sizeof(step.description) - 1] = '\0';
        }
        return step;
    }
};

// =============================================================================
// Tests: Default Configuration — Symbolic Logic Fields
// =============================================================================

TEST_F(ReasoningSymbolicLogicUnit, DefaultConfigEnablesSymbolicLogic) {
    reasoning_engine_config_t cfg = reasoning_engine_default_config();

    EXPECT_TRUE(cfg.enable_symbolic_logic)
        << "Symbolic logic should be enabled by default";
    EXPECT_GT(cfg.symbolic_inference_depth, 0u)
        << "Default symbolic inference depth should be positive";
}

// =============================================================================
// Tests: Step Type Enum — Symbolic Logic
// =============================================================================

TEST_F(ReasoningSymbolicLogicUnit, SymbolicLogicStepTypeIsDistinct) {
    // All known step types including the new SYMBOLIC_LOGIC
    int types[] = {
        REASONING_STEP_RECALL, REASONING_STEP_KNOWLEDGE, REASONING_STEP_INFERENCE,
        REASONING_STEP_VERIFICATION, REASONING_STEP_UNCERTAINTY,
        REASONING_STEP_ANALOGY, REASONING_STEP_DECOMPOSITION,
        REASONING_STEP_SYNTHESIS, REASONING_STEP_WORLD_MODEL,
        REASONING_STEP_JEPA_PREDICTION, REASONING_STEP_SYMBOLIC_LOGIC
    };
    int count = sizeof(types) / sizeof(types[0]);

    // SYMBOLIC_LOGIC is the last entry; verify it differs from every other type
    for (int i = 0; i < count - 1; i++) {
        EXPECT_NE(types[i], static_cast<int>(REASONING_STEP_SYMBOLIC_LOGIC))
            << "REASONING_STEP_SYMBOLIC_LOGIC must differ from type at index " << i;
    }
}

TEST_F(ReasoningSymbolicLogicUnit, SymbolicLogicStepTypeName) {
    const char* name = reasoning_step_type_name(REASONING_STEP_SYMBOLIC_LOGIC);
    ASSERT_NE(name, nullptr);
    EXPECT_STREQ(name, "SYMBOLIC_LOGIC")
        << "Step type name for SYMBOLIC_LOGIC should be \"SYMBOLIC_LOGIC\"";
}

// =============================================================================
// Tests: Chain Operations with Symbolic Logic Steps
// =============================================================================

TEST_F(ReasoningSymbolicLogicUnit, ChainCanHoldSymbolicLogicSteps) {
    reasoning_chain_init(&chain);

    reasoning_step_t step = make_step(REASONING_STEP_SYMBOLIC_LOGIC, 0.85f, 0.90f,
                                       "Modus ponens: if P then Q; P; therefore Q");
    int rc = reasoning_chain_add_step(&chain, &step);
    EXPECT_EQ(rc, 0) << "Adding a SYMBOLIC_LOGIC step should succeed";
    EXPECT_EQ(chain.num_steps, 1u);

    const reasoning_step_t* retrieved = reasoning_chain_get_step(&chain, 0);
    ASSERT_NE(retrieved, nullptr);
    EXPECT_EQ(retrieved->type, REASONING_STEP_SYMBOLIC_LOGIC);
    EXPECT_FLOAT_EQ(retrieved->confidence, 0.85f);
    EXPECT_FLOAT_EQ(retrieved->relevance, 0.90f);
}

TEST_F(ReasoningSymbolicLogicUnit, MixedChainWithSymbolicSteps) {
    reasoning_chain_init(&chain);

    const reasoning_step_type_t types[] = {
        REASONING_STEP_RECALL,
        REASONING_STEP_KNOWLEDGE,
        REASONING_STEP_SYMBOLIC_LOGIC,
        REASONING_STEP_INFERENCE,
        REASONING_STEP_SYMBOLIC_LOGIC,
        REASONING_STEP_VERIFICATION,
        REASONING_STEP_SYNTHESIS
    };
    const int count = sizeof(types) / sizeof(types[0]);

    for (int i = 0; i < count; i++) {
        char desc[64];
        snprintf(desc, sizeof(desc), "Mixed step %d", i);
        reasoning_step_t step = make_step(types[i], 0.5f + i * 0.05f,
                                           0.6f + i * 0.04f, desc);
        int rc = reasoning_chain_add_step(&chain, &step);
        EXPECT_EQ(rc, 0) << "Failed to add step " << i;
    }

    EXPECT_EQ(chain.num_steps, static_cast<uint32_t>(count));

    // Verify all steps are retrievable with correct types
    for (int i = 0; i < count; i++) {
        const reasoning_step_t* retrieved = reasoning_chain_get_step(
            &chain, static_cast<uint32_t>(i));
        ASSERT_NE(retrieved, nullptr) << "Step " << i << " should be retrievable";
        EXPECT_EQ(retrieved->type, types[i])
            << "Step " << i << " type mismatch";
    }

    // Specifically verify the two SYMBOLIC_LOGIC steps
    EXPECT_EQ(reasoning_chain_get_step(&chain, 2)->type, REASONING_STEP_SYMBOLIC_LOGIC);
    EXPECT_EQ(reasoning_chain_get_step(&chain, 4)->type, REASONING_STEP_SYMBOLIC_LOGIC);
}

// =============================================================================
// Tests: Statistics — Symbolic Fields
// =============================================================================

TEST_F(ReasoningSymbolicLogicUnit, StatsInitSymbolicFieldsZero) {
    reasoning_engine_config_t cfg = reasoning_engine_default_config();
    engine = reasoning_engine_create(&cfg);
    ASSERT_NE(engine, nullptr);

    reasoning_engine_stats_t stats;
    memset(&stats, 0xFF, sizeof(stats)); // Poison to detect uninitialized
    int rc = reasoning_engine_get_stats(engine, &stats);
    EXPECT_EQ(rc, 0);

    EXPECT_EQ(stats.symbolic_queries, 0u)
        << "symbolic_queries should start at zero";
    EXPECT_EQ(stats.symbolic_proofs, 0u)
        << "symbolic_proofs should start at zero";
}

// =============================================================================
// Tests: Configuration — Symbolic Logic Options
// =============================================================================

TEST_F(ReasoningSymbolicLogicUnit, ConfigDisableSymbolicLogic) {
    reasoning_engine_config_t cfg = reasoning_engine_default_config();
    cfg.enable_symbolic_logic = false;

    engine = reasoning_engine_create(&cfg);
    EXPECT_NE(engine, nullptr)
        << "Engine creation with symbolic logic disabled should succeed";
}

TEST_F(ReasoningSymbolicLogicUnit, SymbolicInferenceDepthConfigurable) {
    reasoning_engine_config_t cfg = reasoning_engine_default_config();

    // Test various depth values
    uint32_t test_depths[] = {1, 3, 5, 10, 50, 100};
    for (uint32_t depth : test_depths) {
        cfg.symbolic_inference_depth = depth;
        EXPECT_EQ(cfg.symbolic_inference_depth, depth)
            << "Config should store symbolic_inference_depth=" << depth;
    }

    // Verify the last value persists through engine creation
    cfg.symbolic_inference_depth = 42;
    reasoning_engine_t* eng = reasoning_engine_create(&cfg);
    ASSERT_NE(eng, nullptr);
    // Engine was created successfully with custom depth — cleanup
    reasoning_engine_destroy(eng);
}

// =============================================================================
// Tests: Multiple Symbolic Steps in Chain
// =============================================================================

TEST_F(ReasoningSymbolicLogicUnit, ChainMultipleSymbolicSteps) {
    reasoning_chain_init(&chain);

    // Simulate a multi-phase symbolic reasoning:
    // 1) Query phase: formalize the problem
    // 2) Inference phase: apply logical rules
    // 3) Proof phase: verify conclusion
    const char* descriptions[] = {
        "Formalize: All humans are mortal; Socrates is human",
        "Apply modus ponens: P->Q, P |- Q",
        "Verify proof: Socrates is mortal (QED)"
    };
    float confidences[] = {0.70f, 0.85f, 0.95f};

    for (int i = 0; i < 3; i++) {
        reasoning_step_t step = make_step(REASONING_STEP_SYMBOLIC_LOGIC,
                                           confidences[i], 0.9f, descriptions[i]);
        int rc = reasoning_chain_add_step(&chain, &step);
        EXPECT_EQ(rc, 0) << "Failed to add symbolic step " << i;
    }

    EXPECT_EQ(chain.num_steps, 3u);

    // Verify each step preserved its data
    for (uint32_t i = 0; i < 3; i++) {
        const reasoning_step_t* retrieved = reasoning_chain_get_step(&chain, i);
        ASSERT_NE(retrieved, nullptr);
        EXPECT_EQ(retrieved->type, REASONING_STEP_SYMBOLIC_LOGIC);
        EXPECT_FLOAT_EQ(retrieved->confidence, confidences[i]);
        EXPECT_STREQ(retrieved->description, descriptions[i]);
    }
}

// =============================================================================
// Tests: Step Description Preservation
// =============================================================================

TEST_F(ReasoningSymbolicLogicUnit, SymbolicStepDescriptionPreserved) {
    reasoning_chain_init(&chain);

    const char* desc = "Formal proof: (A ^ B) -> C, given A=true, B=true, therefore C=true";
    reasoning_step_t step = make_step(REASONING_STEP_SYMBOLIC_LOGIC, 0.92f, 0.88f, desc);
    int rc = reasoning_chain_add_step(&chain, &step);
    EXPECT_EQ(rc, 0);

    const reasoning_step_t* retrieved = reasoning_chain_get_step(&chain, 0);
    ASSERT_NE(retrieved, nullptr);
    EXPECT_STREQ(retrieved->description, desc)
        << "Symbolic step description should be preserved exactly";
}

// =============================================================================
// Tests: Confidence Range Storage
// =============================================================================

TEST_F(ReasoningSymbolicLogicUnit, SymbolicStepConfidenceInRange) {
    reasoning_chain_init(&chain);

    float test_confidences[] = {0.0f, 0.5f, 0.95f, 1.0f};
    int count = sizeof(test_confidences) / sizeof(test_confidences[0]);

    for (int i = 0; i < count; i++) {
        char desc[64];
        snprintf(desc, sizeof(desc), "Confidence test step %d", i);
        reasoning_step_t step = make_step(REASONING_STEP_SYMBOLIC_LOGIC,
                                           test_confidences[i], 0.5f, desc);
        int rc = reasoning_chain_add_step(&chain, &step);
        EXPECT_EQ(rc, 0) << "Failed to add step with confidence " << test_confidences[i];
    }

    EXPECT_EQ(chain.num_steps, static_cast<uint32_t>(count));

    for (int i = 0; i < count; i++) {
        const reasoning_step_t* retrieved = reasoning_chain_get_step(
            &chain, static_cast<uint32_t>(i));
        ASSERT_NE(retrieved, nullptr);
        EXPECT_FLOAT_EQ(retrieved->confidence, test_confidences[i])
            << "Confidence " << test_confidences[i] << " not stored correctly";
        EXPECT_GE(retrieved->confidence, 0.0f);
        EXPECT_LE(retrieved->confidence, 1.0f);
    }
}

// =============================================================================
// Tests: Engine Without Brain — Symbolic Logic Enabled
// =============================================================================

TEST_F(ReasoningSymbolicLogicUnit, ReasonWithoutBrainStillReturnsError) {
    reasoning_engine_config_t cfg = reasoning_engine_default_config();
    ASSERT_TRUE(cfg.enable_symbolic_logic)
        << "Precondition: symbolic logic should be enabled by default";

    engine = reasoning_engine_create(&cfg);
    ASSERT_NE(engine, nullptr);

    reasoning_chain_init(&chain);

    // Attempting to reason without a connected brain must fail,
    // regardless of symbolic logic being enabled
    int rc = reasoning_engine_reason(engine, "Is P ^ Q -> P valid?", &chain);
    EXPECT_NE(rc, 0) << "Reasoning without a connected brain must return an error";
}
