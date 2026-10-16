/**
 * @file test_reasoning_brain_integration.cpp
 * @brief Integration tests for reasoning engine connected to a live brain
 *
 * WHAT: Tests the reasoning engine operating against a real brain instance
 * WHY:  Unit tests verify chain management in isolation; integration tests
 *       verify that reasoning actually queries brain state, fires engram recall,
 *       and produces domain-aware conclusions
 * HOW:  Creates a BRAIN_SIZE_SMALL brain, connects the reasoning engine,
 *       runs queries, and validates chain contents and statistics
 *
 * @author NIMCP Development Team
 * @date 2026-02-25
 * @version 1.0.0
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <string>

extern "C" {
#include "core/brain/nimcp_brain.h"
#include "cognitive/reasoning/nimcp_reasoning_chain.h"
}

// =============================================================================
// Constants
// =============================================================================

static constexpr uint32_t INPUT_DIM = 32;
static constexpr uint32_t OUTPUT_DIM = 8;

// =============================================================================
// Test Fixture
// =============================================================================

class ReasoningBrainIntegration : public ::testing::Test {
protected:
    brain_t brain = nullptr;
    reasoning_engine_t* engine = nullptr;

    void SetUp() override {
        brain = brain_create("reasoning_test", BRAIN_SIZE_SMALL,
                            BRAIN_TASK_CLASSIFICATION, INPUT_DIM, OUTPUT_DIM);
        ASSERT_NE(brain, nullptr)
            << "Brain creation must succeed for integration tests";

        reasoning_engine_config_t cfg = reasoning_engine_default_config();
        cfg.enable_engram_recall = true;
        cfg.enable_knowledge_query = true;
        cfg.enable_predictive_verify = true;
        cfg.enable_epistemic_check = true;
        engine = reasoning_engine_create(&cfg);
        ASSERT_NE(engine, nullptr)
            << "Reasoning engine creation must succeed";
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
// Tests: Connection
// =============================================================================

TEST_F(ReasoningBrainIntegration, ConnectBrainSucceeds) {
    int rc = reasoning_engine_connect_brain(engine, brain);
    EXPECT_EQ(rc, 0) << "Connecting a valid brain to reasoning engine should succeed";
}

// =============================================================================
// Tests: Reasoning with Connected Brain
// =============================================================================

TEST_F(ReasoningBrainIntegration, ReasonProducesChain) {
    int rc = reasoning_engine_connect_brain(engine, brain);
    ASSERT_EQ(rc, 0);

    reasoning_chain_t chain;
    reasoning_chain_init(&chain);

    rc = reasoning_engine_reason(engine, "What patterns exist in the input data?", &chain);
    EXPECT_EQ(rc, 0) << "Reasoning with a connected brain should succeed";
    EXPECT_GT(chain.num_steps, 0u)
        << "Reasoning should produce at least one chain step";

    // Verify steps have valid types
    for (uint32_t i = 0; i < chain.num_steps; i++) {
        const reasoning_step_t* step = reasoning_chain_get_step(&chain, i);
        ASSERT_NE(step, nullptr);
        EXPECT_GE(static_cast<int>(step->type), static_cast<int>(REASONING_STEP_RECALL));
        EXPECT_LE(static_cast<int>(step->type), static_cast<int>(REASONING_STEP_VISUOSPATIAL));
    }

    reasoning_chain_cleanup(&chain);
}

TEST_F(ReasoningBrainIntegration, ReasonChainHasConclusion) {
    reasoning_engine_connect_brain(engine, brain);

    reasoning_chain_t chain;
    reasoning_chain_init(&chain);

    reasoning_engine_reason(engine, "Is the system healthy?", &chain);

    // After reasoning, the conclusion should be set
    EXPECT_GT(strlen(chain.conclusion), 0u)
        << "Reasoning chain should have a non-empty conclusion";

    // The chain should be marked as complete
    EXPECT_TRUE(chain.is_complete)
        << "After successful reasoning, chain should be marked complete";

    reasoning_chain_cleanup(&chain);
}

TEST_F(ReasoningBrainIntegration, ReasonChainConfidenceInRange) {
    reasoning_engine_connect_brain(engine, brain);

    reasoning_chain_t chain;
    reasoning_chain_init(&chain);

    reasoning_engine_reason(engine, "What is the classification output?", &chain);

    float confidence = reasoning_chain_get_confidence(&chain);
    EXPECT_GE(confidence, 0.0f)
        << "Overall confidence must be non-negative";
    EXPECT_LE(confidence, 1.0f)
        << "Overall confidence must not exceed 1.0";

    // Individual step confidences should also be in range
    for (uint32_t i = 0; i < chain.num_steps; i++) {
        const reasoning_step_t* step = reasoning_chain_get_step(&chain, i);
        ASSERT_NE(step, nullptr);
        EXPECT_GE(step->confidence, 0.0f);
        EXPECT_LE(step->confidence, 1.0f);
    }

    reasoning_chain_cleanup(&chain);
}

// =============================================================================
// Tests: Statistics
// =============================================================================

TEST_F(ReasoningBrainIntegration, ReasonUpdatesStats) {
    reasoning_engine_connect_brain(engine, brain);

    reasoning_chain_t chain;
    reasoning_chain_init(&chain);

    reasoning_engine_reason(engine, "Test query for stats", &chain);

    reasoning_engine_stats_t stats;
    int rc = reasoning_engine_get_stats(engine, &stats);
    EXPECT_EQ(rc, 0);
    EXPECT_GE(stats.total_queries, 1u)
        << "After one reasoning call, total_queries should be >= 1";
    EXPECT_GT(stats.total_steps, 0u)
        << "After reasoning, total_steps should be > 0";

    reasoning_chain_cleanup(&chain);
}

TEST_F(ReasoningBrainIntegration, MultipleQueriesAccumulateStats) {
    reasoning_engine_connect_brain(engine, brain);

    const char* queries[] = {
        "What is the primary pattern?",
        "How confident is the classification?",
        "Are there anomalies in the data?"
    };

    for (int i = 0; i < 3; i++) {
        reasoning_chain_t chain;
        reasoning_chain_init(&chain);
        reasoning_engine_reason(engine, queries[i], &chain);
        reasoning_chain_cleanup(&chain);
    }

    reasoning_engine_stats_t stats;
    reasoning_engine_get_stats(engine, &stats);
    EXPECT_EQ(stats.total_queries, 3u)
        << "After 3 queries, stats.total_queries should be 3";
}

// =============================================================================
// Tests: Domain-Specific Reasoning
// =============================================================================

TEST_F(ReasoningBrainIntegration, ReasonInDomainWorks) {
    reasoning_engine_connect_brain(engine, brain);

    reasoning_chain_t chain;
    reasoning_chain_init(&chain);

    // Domain 0 typically means general/science — exact value depends on impl
    int rc = reasoning_engine_reason_in_domain(engine,
        "What is the dominant frequency?", 0, &chain);
    EXPECT_EQ(rc, 0) << "Domain-specific reasoning should succeed";
    EXPECT_GT(chain.num_steps, 0u);

    reasoning_chain_cleanup(&chain);
}

// =============================================================================
// Tests: Error Handling
// =============================================================================

TEST_F(ReasoningBrainIntegration, DisconnectedEngineReturnsError) {
    // Do NOT connect brain
    reasoning_chain_t chain;
    reasoning_chain_init(&chain);

    int rc = reasoning_engine_reason(engine, "This should fail", &chain);
    EXPECT_NE(rc, 0)
        << "Reasoning without a connected brain should return an error";

    reasoning_chain_cleanup(&chain);
}
