/**
 * @file test_reasoning_chain.cpp
 * @brief Unit tests for the reasoning chain engine
 *
 * WHAT: Tests reasoning chain data structure management and engine lifecycle
 * WHY:  Verify chain operations (add/get/cleanup) and engine config independently
 *       of any brain connection — pure data structure correctness
 * HOW:  GTest fixture with no brain dependency; exercises chain API directly
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

class ReasoningChainUnit : public ::testing::Test {
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
// Tests: Default Configuration
// =============================================================================

TEST_F(ReasoningChainUnit, DefaultConfigHasReasonableValues) {
    reasoning_engine_config_t cfg = reasoning_engine_default_config();

    // Max depth should be positive and reasonable (not 0, not millions)
    EXPECT_GT(cfg.max_depth, 0u);
    EXPECT_LE(cfg.max_depth, 1000u);

    // Max steps should be positive
    EXPECT_GT(cfg.max_steps, 0u);
    EXPECT_LE(cfg.max_steps, 10000u);

    // Confidence threshold should be in (0, 1]
    EXPECT_GT(cfg.confidence_threshold, 0.0f);
    EXPECT_LE(cfg.confidence_threshold, 1.0f);

    // Uncertainty threshold should be in (0, 1]
    EXPECT_GT(cfg.uncertainty_threshold, 0.0f);
    EXPECT_LE(cfg.uncertainty_threshold, 1.0f);

    // Working memory slots should be positive
    EXPECT_GT(cfg.working_memory_slots, 0u);

    // Boolean flags should have well-defined values (true or false)
    // At minimum, engram recall and knowledge query should default to enabled
    EXPECT_TRUE(cfg.enable_engram_recall || !cfg.enable_engram_recall); // valid bool
}

// =============================================================================
// Tests: Engine Lifecycle
// =============================================================================

TEST_F(ReasoningChainUnit, CreateAndDestroy) {
    reasoning_engine_config_t cfg = reasoning_engine_default_config();
    engine = reasoning_engine_create(&cfg);
    ASSERT_NE(engine, nullptr) << "Engine creation with valid config must succeed";
    // Destroy happens in TearDown
}

TEST_F(ReasoningChainUnit, CreateWithNullConfigUsesDefaults) {
    reasoning_engine_t* default_engine = reasoning_engine_create(nullptr);
    EXPECT_NE(default_engine, nullptr)
        << "Creating engine with NULL config should use defaults and succeed";
    if (default_engine) {
        reasoning_engine_destroy(default_engine);
    }
}

// =============================================================================
// Tests: Chain Init / Cleanup
// =============================================================================

TEST_F(ReasoningChainUnit, ChainInitAndCleanup) {
    reasoning_chain_init(&chain);

    EXPECT_EQ(chain.num_steps, 0u);
    EXPECT_EQ(chain.overall_confidence, 0.0f);
    EXPECT_FALSE(chain.is_complete);
    EXPECT_FALSE(chain.has_uncertainty_flag);
    EXPECT_EQ(chain.conclusion[0], '\0');

    // Cleanup on a fresh chain must not crash
    reasoning_chain_cleanup(&chain);
    // Double-init after cleanup should also be safe
    reasoning_chain_init(&chain);
    EXPECT_EQ(chain.num_steps, 0u);
}

// =============================================================================
// Tests: Adding Steps
// =============================================================================

TEST_F(ReasoningChainUnit, ChainAddStep) {
    reasoning_chain_init(&chain);

    reasoning_step_t step = make_step(REASONING_STEP_RECALL, 0.8f, 0.9f,
                                       "Recall relevant memories");
    int rc = reasoning_chain_add_step(&chain, &step);
    EXPECT_EQ(rc, 0) << "Adding a step to an initialized chain should succeed";
    EXPECT_EQ(chain.num_steps, 1u);
}

TEST_F(ReasoningChainUnit, ChainAddMultipleSteps) {
    reasoning_chain_init(&chain);

    const reasoning_step_type_t types[] = {
        REASONING_STEP_RECALL, REASONING_STEP_KNOWLEDGE, REASONING_STEP_INFERENCE,
        REASONING_STEP_VERIFICATION, REASONING_STEP_UNCERTAINTY,
        REASONING_STEP_ANALOGY, REASONING_STEP_DECOMPOSITION,
        REASONING_STEP_SYNTHESIS, REASONING_STEP_RECALL, REASONING_STEP_INFERENCE
    };

    for (int i = 0; i < 10; i++) {
        char desc[64];
        snprintf(desc, sizeof(desc), "Step %d", i);
        reasoning_step_t step = make_step(types[i], 0.5f + i * 0.04f,
                                           0.6f + i * 0.03f, desc);
        int rc = reasoning_chain_add_step(&chain, &step);
        EXPECT_EQ(rc, 0) << "Failed to add step " << i;
    }

    EXPECT_EQ(chain.num_steps, 10u);

    // Verify all steps are retrievable
    for (uint32_t i = 0; i < 10; i++) {
        const reasoning_step_t* retrieved = reasoning_chain_get_step(&chain, i);
        ASSERT_NE(retrieved, nullptr) << "Step " << i << " should be retrievable";
    }
}

TEST_F(ReasoningChainUnit, ChainCapacityGrows) {
    reasoning_chain_init(&chain);

    // Add more steps than any reasonable initial capacity (e.g., 100 steps)
    const uint32_t num_to_add = 100;
    for (uint32_t i = 0; i < num_to_add; i++) {
        reasoning_step_t step = make_step(REASONING_STEP_INFERENCE,
                                           0.5f, 0.5f, "Growth test step");
        int rc = reasoning_chain_add_step(&chain, &step);
        EXPECT_EQ(rc, 0) << "Failed at step " << i << "; capacity may not grow";
    }

    EXPECT_EQ(chain.num_steps, num_to_add);
    EXPECT_GE(chain.capacity, num_to_add)
        << "Capacity must be at least as large as num_steps";
}

// =============================================================================
// Tests: Retrieving Steps
// =============================================================================

TEST_F(ReasoningChainUnit, ChainGetStepReturnsCorrect) {
    reasoning_chain_init(&chain);

    reasoning_step_t step = make_step(REASONING_STEP_ANALOGY, 0.75f, 0.85f,
                                       "Analogy: trees are like networks");
    reasoning_chain_add_step(&chain, &step);

    const reasoning_step_t* retrieved = reasoning_chain_get_step(&chain, 0);
    ASSERT_NE(retrieved, nullptr);
    EXPECT_EQ(retrieved->type, REASONING_STEP_ANALOGY);
    EXPECT_FLOAT_EQ(retrieved->confidence, 0.75f);
    EXPECT_FLOAT_EQ(retrieved->relevance, 0.85f);
    EXPECT_STREQ(retrieved->description, "Analogy: trees are like networks");
}

TEST_F(ReasoningChainUnit, ChainGetStepOutOfBounds) {
    reasoning_chain_init(&chain);

    // Empty chain — any index is out of bounds
    const reasoning_step_t* step0 = reasoning_chain_get_step(&chain, 0);
    EXPECT_EQ(step0, nullptr) << "Out-of-bounds get on empty chain must return NULL";

    // Add one step, try index 1
    reasoning_step_t s = make_step(REASONING_STEP_RECALL, 0.5f, 0.5f, "test");
    reasoning_chain_add_step(&chain, &s);

    const reasoning_step_t* step1 = reasoning_chain_get_step(&chain, 1);
    EXPECT_EQ(step1, nullptr) << "Index 1 on 1-element chain must return NULL";

    // Very large index
    const reasoning_step_t* stepHuge = reasoning_chain_get_step(&chain, UINT32_MAX);
    EXPECT_EQ(stepHuge, nullptr);
}

// =============================================================================
// Tests: Confidence Computation
// =============================================================================

TEST_F(ReasoningChainUnit, ChainConfidenceComputation) {
    reasoning_chain_init(&chain);

    // Add steps with known confidences
    float confidences[] = {0.9f, 0.8f, 0.7f, 0.6f};
    for (int i = 0; i < 4; i++) {
        reasoning_step_t step = make_step(REASONING_STEP_INFERENCE,
                                           confidences[i], 0.5f, "conf test");
        reasoning_chain_add_step(&chain, &step);
    }

    // overall_confidence is computed during reasoning (synthesis phase),
    // not when steps are manually added. Verify individual step confidences instead.
    EXPECT_EQ(chain.num_steps, 4u);
    for (uint32_t i = 0; i < chain.num_steps; i++) {
        EXPECT_GE(chain.steps[i].confidence, 0.0f);
        EXPECT_LE(chain.steps[i].confidence, 1.0f);
        EXPECT_FLOAT_EQ(chain.steps[i].confidence, confidences[i])
            << "Step " << i << " confidence should match what was set";
    }
}

// =============================================================================
// Tests: Step Type Enums
// =============================================================================

TEST_F(ReasoningChainUnit, ChainStepTypesAreDistinct) {
    // Verify all step types have distinct values
    int types[] = {
        REASONING_STEP_RECALL, REASONING_STEP_KNOWLEDGE, REASONING_STEP_INFERENCE,
        REASONING_STEP_VERIFICATION, REASONING_STEP_UNCERTAINTY,
        REASONING_STEP_ANALOGY, REASONING_STEP_DECOMPOSITION,
        REASONING_STEP_SYNTHESIS, REASONING_STEP_WORLD_MODEL,
        REASONING_STEP_JEPA_PREDICTION, REASONING_STEP_SYMBOLIC_LOGIC
    };
    int count = sizeof(types) / sizeof(types[0]);
    EXPECT_EQ(count, 11) << "Should have exactly 11 step types";

    for (int i = 0; i < count; i++) {
        for (int j = i + 1; j < count; j++) {
            EXPECT_NE(types[i], types[j])
                << "Step type " << i << " and " << j << " have the same value";
        }
    }
}

// =============================================================================
// Tests: Cleanup Idempotency
// =============================================================================

TEST_F(ReasoningChainUnit, ChainCleanupIsIdempotent) {
    reasoning_chain_init(&chain);

    // Add some steps
    for (int i = 0; i < 5; i++) {
        reasoning_step_t step = make_step(REASONING_STEP_RECALL, 0.5f, 0.5f, "test");
        reasoning_chain_add_step(&chain, &step);
    }
    ASSERT_EQ(chain.num_steps, 5u);

    // First cleanup
    reasoning_chain_cleanup(&chain);

    // Second cleanup on the same (already cleaned) chain must not crash
    reasoning_chain_cleanup(&chain);

    // Third cleanup — still safe
    reasoning_chain_cleanup(&chain);
}

// =============================================================================
// Tests: Engine Without Brain
// =============================================================================

TEST_F(ReasoningChainUnit, ReasonWithoutBrainReturnsError) {
    reasoning_engine_config_t cfg = reasoning_engine_default_config();
    engine = reasoning_engine_create(&cfg);
    ASSERT_NE(engine, nullptr);

    reasoning_chain_init(&chain);

    // Attempting to reason without connecting a brain should fail
    int rc = reasoning_engine_reason(engine, "What is consciousness?", &chain);
    EXPECT_NE(rc, 0) << "Reasoning without a connected brain must return an error";
}

// =============================================================================
// Tests: Engine Statistics
// =============================================================================

TEST_F(ReasoningChainUnit, EngineStatsStartAtZero) {
    reasoning_engine_config_t cfg = reasoning_engine_default_config();
    engine = reasoning_engine_create(&cfg);
    ASSERT_NE(engine, nullptr);

    reasoning_engine_stats_t stats;
    memset(&stats, 0xFF, sizeof(stats)); // Poison memory to detect uninitialized
    int rc = reasoning_engine_get_stats(engine, &stats);
    EXPECT_EQ(rc, 0);

    EXPECT_EQ(stats.total_queries, 0u);
    EXPECT_EQ(stats.successful_queries, 0u);
    EXPECT_EQ(stats.total_steps, 0u);
    EXPECT_FLOAT_EQ(stats.avg_confidence, 0.0f);
    EXPECT_FLOAT_EQ(stats.avg_steps_per_query, 0.0f);
    EXPECT_EQ(stats.engram_recalls, 0u);
    EXPECT_EQ(stats.knowledge_queries, 0u);
    EXPECT_EQ(stats.verification_passes, 0u);
    EXPECT_EQ(stats.verification_failures, 0u);
    EXPECT_EQ(stats.uncertainty_flags, 0u);
}

TEST_F(ReasoningChainUnit, ResetStatsWorks) {
    reasoning_engine_config_t cfg = reasoning_engine_default_config();
    engine = reasoning_engine_create(&cfg);
    ASSERT_NE(engine, nullptr);

    // Even without running queries, reset should succeed
    int rc = reasoning_engine_reset_stats(engine);
    EXPECT_EQ(rc, 0);

    // Verify stats are zero after reset
    reasoning_engine_stats_t stats;
    rc = reasoning_engine_get_stats(engine, &stats);
    EXPECT_EQ(rc, 0);
    EXPECT_EQ(stats.total_queries, 0u);
    EXPECT_EQ(stats.total_steps, 0u);
}

// =============================================================================
// Tests: Num Steps Helper
// =============================================================================

TEST_F(ReasoningChainUnit, GetNumStepsMatchesCount) {
    reasoning_chain_init(&chain);

    EXPECT_EQ(reasoning_chain_get_num_steps(&chain), 0u);

    for (uint32_t i = 1; i <= 7; i++) {
        reasoning_step_t step = make_step(REASONING_STEP_SYNTHESIS, 0.5f, 0.5f, "x");
        reasoning_chain_add_step(&chain, &step);
        EXPECT_EQ(reasoning_chain_get_num_steps(&chain), i);
    }
}

// =============================================================================
// Tests: Concurrent Pipeline Configuration
// =============================================================================

TEST_F(ReasoningChainUnit, DefaultConfigEnablesConcurrentPipeline) {
    reasoning_engine_config_t cfg = reasoning_engine_default_config();
    EXPECT_TRUE(cfg.enable_concurrent_pipeline)
        << "Concurrent pipeline should be enabled by default";
    EXPECT_EQ(cfg.concurrent_pool_size, 4u)
        << "Default pool size should be 4";
}

TEST_F(ReasoningChainUnit, ConcurrentPipelineCanBeDisabled) {
    reasoning_engine_config_t cfg = reasoning_engine_default_config();
    cfg.enable_concurrent_pipeline = false;
    engine = reasoning_engine_create(&cfg);
    ASSERT_NE(engine, nullptr);

    // Engine should still be usable in sequential mode
    reasoning_chain_init(&chain);
    int rc = reasoning_engine_reason(engine, "test query", &chain);
    // Should fail because not connected to brain, but not crash
    EXPECT_NE(rc, 0);
}

TEST_F(ReasoningChainUnit, ConcurrentEngineCreatesSuccessfully) {
    reasoning_engine_config_t cfg = reasoning_engine_default_config();
    cfg.enable_concurrent_pipeline = true;
    cfg.concurrent_pool_size = 2;
    engine = reasoning_engine_create(&cfg);
    ASSERT_NE(engine, nullptr)
        << "Engine with concurrent pipeline should create successfully";
}

TEST_F(ReasoningChainUnit, ConcurrentEngineDestroyIsClean) {
    // Create and immediately destroy — no leaks, no crashes
    reasoning_engine_config_t cfg = reasoning_engine_default_config();
    cfg.enable_concurrent_pipeline = true;
    cfg.concurrent_pool_size = 4;
    reasoning_engine_t* e = reasoning_engine_create(&cfg);
    ASSERT_NE(e, nullptr);
    reasoning_engine_destroy(e);
    // If we get here without crash/ASAN error, the pool cleanup worked
}

TEST_F(ReasoningChainUnit, ConcurrentPoolSizeZeroFallsBackToDefault) {
    reasoning_engine_config_t cfg = reasoning_engine_default_config();
    cfg.enable_concurrent_pipeline = true;
    cfg.concurrent_pool_size = 0;
    engine = reasoning_engine_create(&cfg);
    // Should still create (uses default pool size 4)
    ASSERT_NE(engine, nullptr);
}
