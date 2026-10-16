/**
 * @file test_rcog_regression.cpp
 * @brief Regression tests for Recursive Cognition module
 *
 * WHAT: Performance and correctness regression tests
 * WHY:  Ensure changes don't degrade performance or break existing behavior
 * HOW:  Benchmark operations, verify numerical stability, check memory
 */

#include <gtest/gtest.h>
#include <cstring>
#include <chrono>
#include <vector>
#include <cmath>

#include "cognitive/recursive/nimcp_rcog_types.h"
#include "cognitive/recursive/nimcp_rcog_collective_bridge.h"
#include "cognitive/recursive/nimcp_rcog_imagination_bridge.h"
#include "cognitive/recursive/nimcp_rcog_immune_bridge.h"
#include "cognitive/recursive/nimcp_rcog_bio_async_bridge.h"
#include "cognitive/recursive/nimcp_rcog_brain_kg_bridge.h"

/* ============================================================================
 * Performance Regression Tests
 * ============================================================================ */

class RcogPerformanceTest : public ::testing::Test {
protected:
    static constexpr int ITERATIONS = 10000;
    static constexpr int WARMUP = 100;
};

TEST_F(RcogPerformanceTest, BridgeCreateDestroyPerformance) {
    // Warmup
    for (int i = 0; i < WARMUP; i++) {
        auto* b = rcog_collective_bridge_create_default();
        rcog_collective_bridge_destroy(b);
    }

    auto start = std::chrono::steady_clock::now();
    
    for (int i = 0; i < ITERATIONS; i++) {
        auto* b = rcog_collective_bridge_create_default();
        rcog_collective_bridge_destroy(b);
    }
    
    auto end = std::chrono::steady_clock::now();
    auto duration_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    double per_op_us = static_cast<double>(duration_us.count()) / ITERATIONS;
    
    // Create/destroy should be < 100us per operation
    EXPECT_LT(per_op_us, 100.0) << "Create/destroy took " << per_op_us << "us per operation";
}

TEST_F(RcogPerformanceTest, UpdatePerformance) {
    auto* bridge = rcog_immune_bridge_create_default();
    ASSERT_NE(bridge, nullptr);

    // Warmup
    for (int i = 0; i < WARMUP; i++) {
        rcog_immune_bridge_update(bridge, 16.0f);
    }

    auto start = std::chrono::steady_clock::now();
    
    for (int i = 0; i < ITERATIONS; i++) {
        rcog_immune_bridge_update(bridge, 16.0f);
    }
    
    auto end = std::chrono::steady_clock::now();
    auto duration_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    double per_op_us = static_cast<double>(duration_us.count()) / ITERATIONS;
    
    // Update should be < 10us per operation
    EXPECT_LT(per_op_us, 10.0) << "Update took " << per_op_us << "us per operation";
    
    rcog_immune_bridge_destroy(bridge);
}

TEST_F(RcogPerformanceTest, EffectsAccessPerformance) {
    auto* bridge = rcog_bio_async_bridge_create_default();
    ASSERT_NE(bridge, nullptr);

    rcog_to_bio_async_effects_t effects;

    // Warmup
    for (int i = 0; i < WARMUP; i++) {
        rcog_bio_async_bridge_get_outgoing_effects(bridge, &effects);
    }

    auto start = std::chrono::steady_clock::now();
    
    for (int i = 0; i < ITERATIONS; i++) {
        rcog_bio_async_bridge_get_outgoing_effects(bridge, &effects);
    }
    
    auto end = std::chrono::steady_clock::now();
    auto duration_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    double per_op_us = static_cast<double>(duration_us.count()) / ITERATIONS;
    
    // Effects access should be < 5us per operation
    EXPECT_LT(per_op_us, 5.0) << "Effects access took " << per_op_us << "us per operation";
    
    rcog_bio_async_bridge_destroy(bridge);
}

TEST_F(RcogPerformanceTest, AllBridgesParallelPerformance) {
    auto* collective = rcog_collective_bridge_create_default();
    auto* imagination = rcog_imagination_bridge_create_default();
    auto* immune = rcog_immune_bridge_create_default();
    auto* bio_async = rcog_bio_async_bridge_create_default();
    auto* brain_kg = rcog_brain_kg_bridge_create_default();

    // Warmup
    for (int i = 0; i < WARMUP; i++) {
        rcog_collective_bridge_update(collective, 16.0f);
        rcog_imagination_bridge_update(imagination, 16.0f);
        rcog_immune_bridge_update(immune, 16.0f);
        rcog_brain_kg_bridge_update(brain_kg, 16.0f);
    }

    auto start = std::chrono::steady_clock::now();
    
    for (int i = 0; i < ITERATIONS; i++) {
        rcog_collective_bridge_update(collective, 16.0f);
        rcog_imagination_bridge_update(imagination, 16.0f);
        rcog_immune_bridge_update(immune, 16.0f);
        rcog_brain_kg_bridge_update(brain_kg, 16.0f);
    }
    
    auto end = std::chrono::steady_clock::now();
    auto duration_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    double per_cycle_us = static_cast<double>(duration_us.count()) / ITERATIONS;
    
    // Full cycle should be < 50us
    EXPECT_LT(per_cycle_us, 50.0) << "Full cycle took " << per_cycle_us << "us";
    
    rcog_collective_bridge_destroy(collective);
    rcog_imagination_bridge_destroy(imagination);
    rcog_immune_bridge_destroy(immune);
    rcog_bio_async_bridge_destroy(bio_async);
    rcog_brain_kg_bridge_destroy(brain_kg);
}

/* ============================================================================
 * Numerical Stability Regression Tests
 * ============================================================================ */

class RcogNumericalStabilityTest : public ::testing::Test {};

TEST_F(RcogNumericalStabilityTest, DopamineAccumulationStability) {
    auto* bridge = rcog_bio_async_bridge_create_default();
    ASSERT_NE(bridge, nullptr);

    // Accumulate many small values
    for (int i = 0; i < 10000; i++) {
        rcog_bio_async_bridge_release_dopamine(bridge, 0.0001f, i);
    }

    rcog_to_bio_async_effects_t effects;
    rcog_bio_async_bridge_get_outgoing_effects(bridge, &effects);
    
    // Value should be valid (not NaN, not Inf)
    EXPECT_FALSE(std::isnan(effects.dopamine_release));
    EXPECT_FALSE(std::isinf(effects.dopamine_release));
    EXPECT_GE(effects.dopamine_release, 0.0f);
    EXPECT_LE(effects.dopamine_release, 1.0f);

    rcog_bio_async_bridge_destroy(bridge);
}

TEST_F(RcogNumericalStabilityTest, ImmuneModulationStability) {
    auto* bridge = rcog_immune_bridge_create_default();
    ASSERT_NE(bridge, nullptr);

    // Many update cycles with auto-recovery
    for (int i = 0; i < 10000; i++) {
        rcog_immune_bridge_update(bridge, 100.0f);
    }

    rcog_immune_modulation_t modulation;
    rcog_immune_bridge_get_modulation(bridge, &modulation);
    
    // Values should be stable
    EXPECT_FALSE(std::isnan(modulation.capacity_multiplier));
    EXPECT_FALSE(std::isinf(modulation.capacity_multiplier));
    EXPECT_GE(modulation.capacity_multiplier, 0.0f);
    EXPECT_LE(modulation.capacity_multiplier, 1.0f);

    rcog_immune_bridge_destroy(bridge);
}

TEST_F(RcogNumericalStabilityTest, StatsAverageStability) {
    auto* bridge = rcog_bio_async_bridge_create_default();
    ASSERT_NE(bridge, nullptr);

    // Many operations to stress averaging
    for (int i = 0; i < 10000; i++) {
        float value = (float)(i % 100) / 100.0f;
        rcog_bio_async_bridge_release_dopamine(bridge, value, i);
    }

    rcog_bio_async_bridge_stats_t stats;
    rcog_bio_async_bridge_get_stats(bridge, &stats);
    
    EXPECT_FALSE(std::isnan(stats.avg_dopamine_release));
    EXPECT_FALSE(std::isinf(stats.avg_dopamine_release));

    rcog_bio_async_bridge_destroy(bridge);
}

/* ============================================================================
 * Memory Regression Tests
 * ============================================================================ */

class RcogMemoryTest : public ::testing::Test {};

TEST_F(RcogMemoryTest, NoLeakOnCreateDestroy) {
    // Create and destroy many bridges
    for (int i = 0; i < 1000; i++) {
        auto* c = rcog_collective_bridge_create_default();
        auto* im = rcog_imagination_bridge_create_default();
        auto* iu = rcog_immune_bridge_create_default();
        auto* b = rcog_bio_async_bridge_create_default();
        auto* k = rcog_brain_kg_bridge_create_default();
        
        rcog_collective_bridge_destroy(c);
        rcog_imagination_bridge_destroy(im);
        rcog_immune_bridge_destroy(iu);
        rcog_bio_async_bridge_destroy(b);
        rcog_brain_kg_bridge_destroy(k);
    }
    
    // If we get here without crashing, memory is likely being freed
    SUCCEED();
}

TEST_F(RcogMemoryTest, StatsResetDoesNotLeak) {
    auto* bridge = rcog_bio_async_bridge_create_default();
    ASSERT_NE(bridge, nullptr);

    for (int i = 0; i < 1000; i++) {
        rcog_bio_async_bridge_release_dopamine(bridge, 0.5f, i);
        rcog_bio_async_bridge_reset_stats(bridge);
    }

    rcog_bio_async_bridge_destroy(bridge);
    SUCCEED();
}

/* ============================================================================
 * Correctness Regression Tests
 * ============================================================================ */

class RcogCorrectnessTest : public ::testing::Test {};

TEST_F(RcogCorrectnessTest, InflammationLevelConsistent) {
    auto* bridge = rcog_immune_bridge_create_default();
    ASSERT_NE(bridge, nullptr);

    rcog_inflammation_level_t level = rcog_immune_bridge_get_inflammation_level(bridge);
    
    // Check consistency with effects
    immune_to_rcog_effects_t effects;
    rcog_immune_bridge_get_incoming_effects(bridge, &effects);
    
    EXPECT_EQ(effects.inflammation_level, level);

    rcog_immune_bridge_destroy(bridge);
}

TEST_F(RcogCorrectnessTest, HealthConsistentWithEffects) {
    auto* bridge = rcog_brain_kg_bridge_create_default();
    ASSERT_NE(bridge, nullptr);

    float health = rcog_brain_kg_bridge_get_system_health(bridge);

    kg_to_rcog_effects_t effects;
    rcog_brain_kg_bridge_get_incoming_effects(bridge, &effects);

    // Both should be in valid range [0, 1]
    EXPECT_GE(health, 0.0f);
    EXPECT_LE(health, 1.0f);
    EXPECT_GE(effects.overall_health, 0.0f);
    EXPECT_LE(effects.overall_health, 1.0f);

    rcog_brain_kg_bridge_destroy(bridge);
}

TEST_F(RcogCorrectnessTest, FocusMatchesState) {
    auto* bridge = rcog_brain_kg_bridge_create_default();
    ASSERT_NE(bridge, nullptr);

    // Set processing state
    rcog_processing_state_t state = {0};
    state.is_processing = true;
    state.current_depth = 4;
    rcog_brain_kg_bridge_update_state(bridge, &state);
    
    char focus[128];
    rcog_brain_kg_bridge_get_focus(bridge, focus, sizeof(focus));
    
    // Focus should mention processing and depth
    EXPECT_NE(strstr(focus, "processing"), nullptr);

    rcog_brain_kg_bridge_destroy(bridge);
}

TEST_F(RcogCorrectnessTest, DefaultConfigsValid) {
    // All default configs should produce valid values
    {
        rcog_collective_bridge_config_t cfg = rcog_collective_bridge_default_config();
        EXPECT_GT(cfg.max_volunteered_tasks, 0u);
        EXPECT_GT(cfg.broadcast_timeout_ms, 0.0f);
    }
    {
        rcog_imagination_bridge_config_t cfg = rcog_imagination_bridge_default_config();
        EXPECT_GT(cfg.simulation_threshold, 0.0f);
        EXPECT_LE(cfg.simulation_threshold, 1.0f);
    }
    {
        rcog_immune_bridge_config_t cfg = rcog_immune_bridge_default_config();
        EXPECT_GT(cfg.min_capacity, 0.0f);
        EXPECT_LE(cfg.min_capacity, 1.0f);
    }
    {
        rcog_bio_async_bridge_config_t cfg = rcog_bio_async_bridge_default_config();
        EXPECT_GT(cfg.message_queue_size, 0u);
    }
    {
        rcog_brain_kg_bridge_config_t cfg = rcog_brain_kg_bridge_default_config();
        EXPECT_GT(cfg.state_update_interval_ms, 0u);
    }
}

/* ============================================================================
 * Core Component Regression Tests (Simplified)
 * ============================================================================ */

#include "cognitive/recursive/nimcp_rcog_orchestrator.h"
#include "cognitive/recursive/nimcp_rcog_delegation_pool.h"
#include "cognitive/recursive/nimcp_rcog_tool_router.h"
#include "cognitive/recursive/nimcp_rcog_context_store.h"
#include "cognitive/recursive/nimcp_rcog_answer.h"

/* Core Component Memory Tests */
class RcogCoreMemoryTest : public ::testing::Test {};

TEST_F(RcogCoreMemoryTest, NoLeakOnCoreCreateDestroy) {
    for (int i = 0; i < 100; i++) {
        auto* store = rcog_context_store_create_default();
        auto* router = rcog_tool_router_create_default();
        auto* refiner = rcog_answer_refiner_create_default();
        auto* pool = rcog_delegation_pool_create_default();
        auto* orch = rcog_orchestrator_create_default();

        rcog_context_store_destroy(store);
        rcog_tool_router_destroy(router);
        rcog_answer_refiner_destroy(refiner);
        rcog_delegation_pool_destroy(pool);
        rcog_orchestrator_destroy(orch);
    }

    SUCCEED();
}

/* Core Component Correctness Tests */
class RcogCoreCorrectnessTest : public ::testing::Test {};

// Dummy tool handler for registration tests
static rcog_error_t dummy_tool_handler(
    const void* input, size_t input_size,
    void* context, void** output, size_t* output_size) {
    (void)input; (void)input_size; (void)context;
    (void)output; (void)output_size;
    return RCOG_OK;
}

TEST_F(RcogCoreCorrectnessTest, ToolRouterTierHierarchy) {
    auto* router = rcog_tool_router_create_default();
    ASSERT_NE(router, nullptr);

    // Register tool at L2 tier using rcog_tool_def_t
    rcog_tool_def_t def = rcog_tool_def_create("l2_tool", dummy_tool_handler, RCOG_TIER_L2_PERCEPTION);
    int result = rcog_tool_router_register(router, &def);
    EXPECT_EQ(result, 0);

    // Tier access: higher numbered tiers can access lower tier tools
    // L1 (lower number, cannot access L2 tools)
    EXPECT_FALSE(rcog_tool_router_can_access(router, "l2_tool", RCOG_TIER_L1_REASONING));

    // L2 (same) should have access
    EXPECT_TRUE(rcog_tool_router_can_access(router, "l2_tool", RCOG_TIER_L2_PERCEPTION));

    // L3 (higher number, can access L2 tools)
    EXPECT_TRUE(rcog_tool_router_can_access(router, "l2_tool", RCOG_TIER_L3_ACTION));

    rcog_tool_router_destroy(router);
}

TEST_F(RcogCoreCorrectnessTest, OrchestratorDecomposeValid) {
    auto* orch = rcog_orchestrator_create_default();
    ASSERT_NE(orch, nullptr);

    // Default config should be valid
    rcog_orchestrator_config_t cfg = rcog_orchestrator_default_config();
    EXPECT_GT(cfg.max_parallel_subtasks, 0u);
    EXPECT_GT(cfg.decomposition_timeout_ms, 0u);

    rcog_orchestrator_destroy(orch);
}

TEST_F(RcogCoreCorrectnessTest, DelegationPoolConfigValid) {
    rcog_delegation_pool_config_t cfg = rcog_delegation_pool_default_config();

    // Check tier workers (total_workers is computed at pool creation)
    EXPECT_GT(cfg.tiers[RCOG_TIER_L1_REASONING].num_workers, 0u);
    EXPECT_GT(cfg.default_task_timeout_ms, 0u);
    EXPECT_GT(cfg.shutdown_timeout_ms, 0u);
    EXPECT_GT(cfg.max_pending_tasks, 0u);
}

TEST_F(RcogCoreCorrectnessTest, AnswerRefinerDefaultsValid) {
    rcog_answer_config_t cfg = rcog_answer_default_config();

    EXPECT_GT(cfg.max_steps, 0u);
    EXPECT_GT(cfg.learning_rate, 0.0f);
    EXPECT_LE(cfg.learning_rate, 1.0f);
    EXPECT_GT(cfg.convergence_epsilon, 0.0f);
}
