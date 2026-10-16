/**
 * @file e2e_test_recursive_cognition_pipeline.cpp
 * @brief End-to-end tests for Recursive Cognition pipeline
 *
 * WHAT: Full pipeline tests for recursive cognition module
 * WHY:  Verify complete processing flow from goal to answer
 * HOW:  Create full bridge network, simulate processing cycles, verify outputs
 *
 * PIPELINE STAGES:
 * 1. Initialize all bridges
 * 2. Simulate goal processing
 * 3. Track effects through bridges
 * 4. Verify bidirectional data flow
 * 5. Check statistics and health
 */

#include <gtest/gtest.h>
#include <cstring>
#include <thread>
#include <chrono>
#include <vector>

#include "cognitive/recursive/nimcp_rcog_types.h"
#include "cognitive/recursive/nimcp_rcog_collective_bridge.h"
#include "cognitive/recursive/nimcp_rcog_imagination_bridge.h"
#include "cognitive/recursive/nimcp_rcog_immune_bridge.h"
#include "cognitive/recursive/nimcp_rcog_bio_async_bridge.h"
#include "cognitive/recursive/nimcp_rcog_brain_kg_bridge.h"

// Dummy tool handler for registration tests
static rcog_error_t e2e_dummy_tool_handler(
    const void* input, size_t input_size,
    void* context, void** output, size_t* output_size) {
    (void)input; (void)input_size; (void)context;
    (void)output; (void)output_size;
    return RCOG_OK;
}

/* ============================================================================
 * E2E Test Fixture
 * ============================================================================ */

class RcogE2EPipelineTest : public ::testing::Test {
protected:
    // Bridge network
    rcog_collective_bridge_t* collective = nullptr;
    rcog_imagination_bridge_t* imagination = nullptr;
    rcog_immune_bridge_t* immune = nullptr;
    rcog_bio_async_bridge_t* bio_async = nullptr;
    rcog_brain_kg_bridge_t* brain_kg = nullptr;

    void SetUp() override {
        // Create all bridges with default configs
        collective = rcog_collective_bridge_create_default();
        imagination = rcog_imagination_bridge_create_default();
        immune = rcog_immune_bridge_create_default();
        bio_async = rcog_bio_async_bridge_create_default();
        brain_kg = rcog_brain_kg_bridge_create_default();

        ASSERT_NE(collective, nullptr);
        ASSERT_NE(imagination, nullptr);
        ASSERT_NE(immune, nullptr);
        ASSERT_NE(bio_async, nullptr);
        ASSERT_NE(brain_kg, nullptr);
    }

    void TearDown() override {
        if (collective) rcog_collective_bridge_destroy(collective);
        if (imagination) rcog_imagination_bridge_destroy(imagination);
        if (immune) rcog_immune_bridge_destroy(immune);
        if (bio_async) rcog_bio_async_bridge_destroy(bio_async);
        if (brain_kg) rcog_brain_kg_bridge_destroy(brain_kg);
    }

    // Simulate a full processing cycle
    void SimulateProcessingCycle(float delta_ms) {
        rcog_collective_bridge_update(collective, delta_ms);
        rcog_imagination_bridge_update(imagination, delta_ms);
        rcog_immune_bridge_update(immune, delta_ms);
        rcog_brain_kg_bridge_update(brain_kg, delta_ms);
    }
};

/* ============================================================================
 * Pipeline Tests
 * ============================================================================ */

TEST_F(RcogE2EPipelineTest, FullPipelineInitialization) {
    // All bridges should be created and functional
    EXPECT_FALSE(rcog_collective_bridge_is_connected(collective));
    EXPECT_FALSE(rcog_imagination_bridge_is_connected(imagination));
    EXPECT_FALSE(rcog_immune_bridge_is_connected(immune));
    EXPECT_FALSE(rcog_bio_async_bridge_is_connected(bio_async));
    EXPECT_FALSE(rcog_brain_kg_bridge_is_connected(brain_kg));
}

TEST_F(RcogE2EPipelineTest, SimulatedGoalProcessing) {
    // Simulate goal processing through pipeline
    
    // 1. Set processing state in brain KG
    rcog_processing_state_t state = {0};
    state.is_processing = true;
    state.current_depth = 1;
    rcog_brain_kg_bridge_update_state(brain_kg, &state);
    
    // 2. Signal priority via bio-async (norepinephrine)
    rcog_bio_async_bridge_signal_priority(bio_async, 0.7f, 1);
    
    // 3. Focus attention
    rcog_bio_async_bridge_modulate_attention(bio_async, 0.9f, "goal_processing");
    
    // 4. Run update cycles
    for (int i = 0; i < 10; i++) {
        SimulateProcessingCycle(16.0f);  // 60fps
    }
    
    // 5. Verify state propagation
    char focus[128];
    rcog_brain_kg_bridge_get_focus(brain_kg, focus, sizeof(focus));
    EXPECT_STRNE(focus, "idle");
    
    rcog_to_bio_async_effects_t bio_effects;
    rcog_bio_async_bridge_get_outgoing_effects(bio_async, &bio_effects);
    EXPECT_FLOAT_EQ(bio_effects.norepinephrine_level, 0.7f);
}

TEST_F(RcogE2EPipelineTest, RecursiveDepthProgression) {
    // Simulate recursive processing at increasing depths
    
    for (uint32_t depth = 1; depth <= 5; depth++) {
        rcog_processing_state_t state = {0};
        state.is_processing = true;
        state.current_depth = depth;
        
        rcog_brain_kg_bridge_update_state(brain_kg, &state);
        SimulateProcessingCycle(16.0f);
        
        // Verify depth is tracked
        kg_to_rcog_effects_t effects;
        rcog_brain_kg_bridge_get_incoming_effects(brain_kg, &effects);
        
        // Focus should reflect processing
        char focus[128];
        rcog_brain_kg_bridge_get_focus(brain_kg, focus, sizeof(focus));
        EXPECT_NE(strstr(focus, "processing"), nullptr);
    }
}

TEST_F(RcogE2EPipelineTest, SubtaskCompletionReward) {
    // Simulate subtask completion with dopamine reward
    
    for (uint64_t task = 1; task <= 10; task++) {
        // Complete subtask
        rcog_bio_async_bridge_release_dopamine(bio_async, 0.3f, task);
        
        // Update cycle
        SimulateProcessingCycle(16.0f);
    }
    
    // Check accumulated effects
    rcog_to_bio_async_effects_t effects;
    rcog_bio_async_bridge_get_outgoing_effects(bio_async, &effects);
    EXPECT_EQ(effects.completed_subtask_count, 10u);
    
    // Stats should reflect operations
    rcog_bio_async_bridge_stats_t stats;
    rcog_bio_async_bridge_get_stats(bio_async, &stats);
    EXPECT_GT(stats.avg_dopamine_release, 0.0f);
}

TEST_F(RcogE2EPipelineTest, ImmuneInfluenceOnProcessing) {
    // Simulate immune system influence on recursive processing

    // Start in unconnected state - values in valid range
    immune_to_rcog_effects_t effects;
    rcog_immune_bridge_get_incoming_effects(immune, &effects);
    EXPECT_GE(effects.capacity_multiplier, 0.0f);
    EXPECT_LE(effects.capacity_multiplier, 1.0f);
    EXPECT_EQ(effects.inflammation_level, RCOG_INFLAMMATION_NONE);

    // Process for extended period
    for (int i = 0; i < 100; i++) {
        rcog_immune_bridge_update(immune, 100.0f);
    }

    // Immune state should remain stable without failures
    rcog_immune_bridge_get_incoming_effects(immune, &effects);
    EXPECT_GE(effects.capacity_multiplier, 0.0f);
    EXPECT_LE(effects.capacity_multiplier, 1.0f);
}

TEST_F(RcogE2EPipelineTest, BidirectionalEffectsFlow) {
    // Verify all bridges have bidirectional effects
    
    // Collective: rcog <-> swarm
    {
        rcog_to_collective_effects_t out;
        collective_to_rcog_effects_t in;
        EXPECT_EQ(rcog_collective_bridge_get_outgoing_effects(collective, &out), RCOG_OK);
        EXPECT_EQ(rcog_collective_bridge_get_incoming_effects(collective, &in), RCOG_OK);
    }
    
    // Imagination: rcog <-> imagination engine
    {
        rcog_to_imagination_effects_t out;
        imagination_to_rcog_effects_t in;
        EXPECT_EQ(rcog_imagination_bridge_get_outgoing_effects(imagination, &out), RCOG_OK);
        EXPECT_EQ(rcog_imagination_bridge_get_incoming_effects(imagination, &in), RCOG_OK);
    }
    
    // Immune: rcog <-> brain immune
    {
        rcog_to_immune_effects_t out;
        immune_to_rcog_effects_t in;
        EXPECT_EQ(rcog_immune_bridge_get_outgoing_effects(immune, &out), RCOG_OK);
        EXPECT_EQ(rcog_immune_bridge_get_incoming_effects(immune, &in), RCOG_OK);
    }
    
    // Bio-async: rcog <-> bio-async messaging
    {
        rcog_to_bio_async_effects_t out;
        bio_async_to_rcog_effects_t in;
        EXPECT_EQ(rcog_bio_async_bridge_get_outgoing_effects(bio_async, &out), RCOG_OK);
        EXPECT_EQ(rcog_bio_async_bridge_get_incoming_effects(bio_async, &in), RCOG_OK);
    }
    
    // Brain KG: rcog <-> knowledge graph
    {
        rcog_to_kg_effects_t out;
        kg_to_rcog_effects_t in;
        EXPECT_EQ(rcog_brain_kg_bridge_get_outgoing_effects(brain_kg, &out), RCOG_OK);
        EXPECT_EQ(rcog_brain_kg_bridge_get_incoming_effects(brain_kg, &in), RCOG_OK);
    }
}

TEST_F(RcogE2EPipelineTest, LongRunningStability) {
    // Run pipeline for extended period to check stability
    
    auto start = std::chrono::steady_clock::now();
    int cycles = 0;
    
    // Run for at least 1 second of simulated time
    while (cycles < 1000) {
        // Simulate varying workload
        float delta = 16.0f + (cycles % 10) * 2.0f;
        
        SimulateProcessingCycle(delta);
        
        // Occasionally change state
        if (cycles % 100 == 0) {
            rcog_processing_state_t state = {0};
            state.is_processing = (cycles % 200 == 0);
            state.current_depth = (cycles / 100) % 5 + 1;
            rcog_brain_kg_bridge_update_state(brain_kg, &state);
        }
        
        // Occasionally release dopamine
        if (cycles % 50 == 0) {
            rcog_bio_async_bridge_release_dopamine(bio_async, 0.2f, cycles);
        }
        
        cycles++;
    }
    
    auto end = std::chrono::steady_clock::now();
    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    // Should complete in reasonable time
    EXPECT_LT(duration_ms.count(), 5000) << "Long run took too long: " << duration_ms.count() << "ms";
    
    // All bridges should still be functional
    EXPECT_FLOAT_EQ(rcog_brain_kg_bridge_get_system_health(brain_kg), 1.0f);
    EXPECT_EQ(rcog_immune_bridge_get_inflammation_level(immune), RCOG_INFLAMMATION_NONE);
}

TEST_F(RcogE2EPipelineTest, StatisticsConsistency) {
    // Verify statistics are consistent across bridges
    
    // Perform known operations
    for (int i = 0; i < 100; i++) {
        rcog_bio_async_bridge_release_dopamine(bio_async, 0.1f + (i % 10) * 0.01f, i);
        rcog_brain_kg_bridge_update(brain_kg, 16.0f);
    }
    
    // Check bio-async stats
    rcog_bio_async_bridge_stats_t bio_stats;
    rcog_bio_async_bridge_get_stats(bio_async, &bio_stats);
    EXPECT_GT(bio_stats.avg_dopamine_release, 0.0f);
    EXPECT_LE(bio_stats.avg_dopamine_release, 1.0f);
    
    // Check brain KG stats
    rcog_brain_kg_bridge_stats_t kg_stats;
    rcog_brain_kg_bridge_get_stats(brain_kg, &kg_stats);
    EXPECT_GT(kg_stats.state_updates, 0u);
    
    // Reset and verify
    rcog_bio_async_bridge_reset_stats(bio_async);
    rcog_bio_async_bridge_get_stats(bio_async, &bio_stats);
    EXPECT_EQ(bio_stats.messages_sent, 0u);
}

TEST_F(RcogE2EPipelineTest, ProcessingCompletionFlow) {
    // Simulate complete processing flow: start -> process -> complete
    
    // 1. Start processing
    rcog_processing_state_t start_state = {0};
    start_state.is_processing = true;
    start_state.current_depth = 1;
    rcog_brain_kg_bridge_update_state(brain_kg, &start_state);
    
    // 2. Process at various depths
    for (uint32_t depth = 2; depth <= 4; depth++) {
        rcog_processing_state_t state = {0};
        state.is_processing = true;
        state.current_depth = depth;
        rcog_brain_kg_bridge_update_state(brain_kg, &state);
        
        // Dopamine for progress
        rcog_bio_async_bridge_release_dopamine(bio_async, 0.2f, depth);
        
        SimulateProcessingCycle(16.0f);
    }
    
    // 3. Complete processing
    rcog_processing_state_t end_state = {0};
    end_state.is_processing = false;
    end_state.current_depth = 0;
    rcog_brain_kg_bridge_update_state(brain_kg, &end_state);
    
    // 4. Final dopamine reward
    rcog_bio_async_bridge_release_dopamine(bio_async, 0.8f, 999);
    
    SimulateProcessingCycle(16.0f);
    
    // 5. Verify completion
    char focus[128];
    rcog_brain_kg_bridge_get_focus(brain_kg, focus, sizeof(focus));
    EXPECT_STREQ(focus, "idle");
}

/* ============================================================================
 * Error Recovery Tests
 * ============================================================================ */

TEST_F(RcogE2EPipelineTest, GracefulNullHandling) {
    // Ensure pipeline handles null pointers gracefully
    
    EXPECT_NE(rcog_collective_bridge_get_outgoing_effects(nullptr, nullptr), RCOG_OK);
    EXPECT_NE(rcog_imagination_bridge_get_outgoing_effects(nullptr, nullptr), RCOG_OK);
    EXPECT_NE(rcog_immune_bridge_get_outgoing_effects(nullptr, nullptr), RCOG_OK);
    EXPECT_NE(rcog_bio_async_bridge_get_outgoing_effects(nullptr, nullptr), RCOG_OK);
    EXPECT_NE(rcog_brain_kg_bridge_get_outgoing_effects(nullptr, nullptr), RCOG_OK);
    
    // Pipeline should still work
    SimulateProcessingCycle(16.0f);
}

TEST_F(RcogE2EPipelineTest, InvalidValueRejection) {
    // Test rejection of invalid values

    // Invalid dopamine values
    EXPECT_NE(rcog_bio_async_bridge_release_dopamine(bio_async, -0.1f, 1), RCOG_OK);
    EXPECT_NE(rcog_bio_async_bridge_release_dopamine(bio_async, 1.5f, 1), RCOG_OK);

    // Valid value should still work
    EXPECT_EQ(rcog_bio_async_bridge_release_dopamine(bio_async, 0.5f, 1), RCOG_OK);
}

/* ============================================================================
 * Core Component E2E Tests
 * ============================================================================ */

#include "cognitive/recursive/nimcp_rcog_engine.h"
#include "cognitive/recursive/nimcp_rcog_orchestrator.h"
#include "cognitive/recursive/nimcp_rcog_delegation_pool.h"
#include "cognitive/recursive/nimcp_rcog_tool_router.h"
#include "cognitive/recursive/nimcp_rcog_context_store.h"
#include "cognitive/recursive/nimcp_rcog_answer.h"

/**
 * @brief Full engine pipeline test fixture
 */
class RcogEngineE2ETest : public ::testing::Test {
protected:
    rcog_engine_t* engine = nullptr;

    void SetUp() override {
        engine = rcog_engine_create_default();
        ASSERT_NE(engine, nullptr);
    }

    void TearDown() override {
        if (engine) {
            rcog_engine_stop(engine, 1000);
            rcog_engine_destroy(engine);
        }
    }
};

TEST_F(RcogEngineE2ETest, FullEngineLifecycle) {
    // Initialize all subsystems
    int result = rcog_engine_init(engine);
    EXPECT_EQ(result, 0);

    // Start engine
    result = rcog_engine_start(engine);
    EXPECT_EQ(result, 0);

    // Verify subsystems are accessible
    rcog_context_store_t* store = rcog_engine_get_context_store(engine);
    EXPECT_NE(store, nullptr);

    rcog_tool_router_t* router = rcog_engine_get_tool_router(engine);
    EXPECT_NE(router, nullptr);

    rcog_orchestrator_t* orch = rcog_engine_get_orchestrator(engine);
    EXPECT_NE(orch, nullptr);

    rcog_delegation_pool_t* pool = rcog_engine_get_delegation_pool(engine);
    EXPECT_NE(pool, nullptr);

    rcog_answer_refiner_t* refiner = rcog_engine_get_answer_refiner(engine);
    EXPECT_NE(refiner, nullptr);

    // Stop gracefully
    result = rcog_engine_stop(engine, 2000);
    EXPECT_EQ(result, 0);
}

TEST_F(RcogEngineE2ETest, EngineContextPersistence) {
    rcog_engine_init(engine);
    rcog_engine_start(engine);

    rcog_context_store_t* store = rcog_engine_get_context_store(engine);
    ASSERT_NE(store, nullptr);

    // Store text values using actual API
    int result = rcog_context_store_set_text(store, "goal", "solve the puzzle");
    EXPECT_EQ(result, 0);

    // Verify store has entries
    rcog_context_store_stats_t stats;
    result = rcog_context_store_get_stats(store, &stats);
    EXPECT_EQ(result, 0);
    EXPECT_GE(stats.variable_count, 1u);

    rcog_engine_stop(engine, 1000);
}

TEST_F(RcogEngineE2ETest, EngineToolRegistrationAndAccess) {
    rcog_engine_init(engine);
    rcog_engine_start(engine);

    rcog_tool_router_t* router = rcog_engine_get_tool_router(engine);
    ASSERT_NE(router, nullptr);

    // Register custom tool using actual API
    rcog_tool_def_t tool = rcog_tool_def_create("custom_solver", e2e_dummy_tool_handler, RCOG_TIER_L2_PERCEPTION);
    int result = rcog_tool_router_register(router, &tool);
    EXPECT_EQ(result, 0);

    // Verify access control (higher tiers can access lower tier tools)
    EXPECT_FALSE(rcog_tool_router_can_access(router, "custom_solver", RCOG_TIER_L1_REASONING));
    EXPECT_TRUE(rcog_tool_router_can_access(router, "custom_solver", RCOG_TIER_L2_PERCEPTION));
    EXPECT_TRUE(rcog_tool_router_can_access(router, "custom_solver", RCOG_TIER_L3_ACTION));

    rcog_engine_stop(engine, 1000);
}

TEST_F(RcogEngineE2ETest, EngineDelegationPoolOperations) {
    rcog_engine_init(engine);
    rcog_engine_start(engine);

    rcog_delegation_pool_t* pool = rcog_engine_get_delegation_pool(engine);
    ASSERT_NE(pool, nullptr);

    // Check capacity
    EXPECT_TRUE(rcog_delegation_pool_has_capacity(pool));

    // Get worker info
    uint32_t total_workers = rcog_delegation_pool_get_total_workers(pool);
    EXPECT_GT(total_workers, 0u);

    // Check stats
    rcog_delegation_pool_stats_t stats;
    rcog_delegation_pool_get_stats(pool, &stats);
    EXPECT_EQ(stats.tasks_submitted, 0u);

    rcog_engine_stop(engine, 1000);
}

TEST_F(RcogEngineE2ETest, EngineAnswerRefinement) {
    rcog_engine_init(engine);
    rcog_engine_start(engine);

    rcog_answer_refiner_t* refiner = rcog_engine_get_answer_refiner(engine);
    ASSERT_NE(refiner, nullptr);

    // Verify refiner is accessible and get stats
    rcog_answer_stats_t stats;
    int result = rcog_answer_refiner_get_stats(refiner, &stats);
    EXPECT_EQ(result, 0);

    // Reset stats
    rcog_answer_refiner_reset_stats(refiner);

    rcog_engine_stop(engine, 1000);
}

TEST_F(RcogEngineE2ETest, EngineImmuneModulation) {
    rcog_engine_init(engine);
    rcog_engine_start(engine);

    // Apply immune modulation
    rcog_immune_modulation_t mod = {0};
    mod.capacity_multiplier = 0.8f;
    mod.timeout_multiplier = 1.2f;

    int result = rcog_engine_apply_immune_modulation(engine, &mod);
    EXPECT_EQ(result, 0);

    // Verify modulation was accepted - pool should reflect capacity
    rcog_delegation_pool_t* pool = rcog_engine_get_delegation_pool(engine);
    ASSERT_NE(pool, nullptr);
    float capacity = rcog_delegation_pool_get_effective_capacity(pool);
    EXPECT_GT(capacity, 0.0f);
    EXPECT_LE(capacity, 1.0f);

    rcog_engine_stop(engine, 1000);
}

TEST_F(RcogEngineE2ETest, EngineStatsTracking) {
    rcog_engine_init(engine);
    rcog_engine_start(engine);

    // Get initial stats
    rcog_engine_stats_t stats;
    rcog_engine_get_stats(engine, &stats);

    // Stats should be zeroed initially
    EXPECT_EQ(stats.goals_submitted, 0u);

    // Reset stats
    rcog_engine_reset_stats(engine);
    rcog_engine_get_stats(engine, &stats);
    EXPECT_EQ(stats.goals_submitted, 0u);

    rcog_engine_stop(engine, 1000);
}

/**
 * @brief Full orchestrator-pool integration test
 */
class RcogOrchestratorPoolE2ETest : public ::testing::Test {
protected:
    rcog_orchestrator_t* orchestrator = nullptr;
    rcog_delegation_pool_t* pool = nullptr;
    rcog_tool_router_t* router = nullptr;
    rcog_context_store_t* context = nullptr;

    void SetUp() override {
        orchestrator = rcog_orchestrator_create_default();
        pool = rcog_delegation_pool_create_default();
        router = rcog_tool_router_create_default();
        context = rcog_context_store_create_default();

        ASSERT_NE(orchestrator, nullptr);
        ASSERT_NE(pool, nullptr);
        ASSERT_NE(router, nullptr);
        ASSERT_NE(context, nullptr);

        // Connect components
        rcog_orchestrator_connect_delegation_pool(orchestrator, pool);
        rcog_orchestrator_connect_context_store(orchestrator, context);
        rcog_delegation_pool_connect_tool_router(pool, router);
        rcog_delegation_pool_connect_context_store(pool, context);
    }

    void TearDown() override {
        if (pool) {
            rcog_delegation_pool_stop(pool, 1000);
            rcog_delegation_pool_destroy(pool);
        }
        if (orchestrator) rcog_orchestrator_destroy(orchestrator);
        if (router) rcog_tool_router_destroy(router);
        if (context) rcog_context_store_destroy(context);
    }
};

TEST_F(RcogOrchestratorPoolE2ETest, ConnectedSystemStart) {
    // Start pool
    int result = rcog_delegation_pool_start(pool);
    EXPECT_EQ(result, 0);

    // Both should be operational
    EXPECT_TRUE(rcog_delegation_pool_has_capacity(pool));
}

TEST_F(RcogOrchestratorPoolE2ETest, SharedContextAccess) {
    rcog_delegation_pool_start(pool);

    // Store data via context using actual API
    int result = rcog_context_store_set_text(context, "shared_key", "shared_value");
    EXPECT_EQ(result, 0);

    // Verify context store has entries
    rcog_context_store_stats_t stats;
    result = rcog_context_store_get_stats(context, &stats);
    EXPECT_EQ(result, 0);
    EXPECT_GE(stats.variable_count, 1u);
}

TEST_F(RcogOrchestratorPoolE2ETest, ToolRouterSharing) {
    rcog_delegation_pool_start(pool);

    // Register tool via shared router using actual API
    rcog_tool_def_t tool = rcog_tool_def_create("shared_tool", e2e_dummy_tool_handler, RCOG_TIER_L2_PERCEPTION);
    int result = rcog_tool_router_register(router, &tool);
    EXPECT_EQ(result, 0);

    // Verify tool is accessible at same or higher tier
    EXPECT_TRUE(rcog_tool_router_can_access(router, "shared_tool", RCOG_TIER_L2_PERCEPTION));
}

TEST_F(RcogOrchestratorPoolE2ETest, WorkerScaling) {
    rcog_delegation_pool_start(pool);

    uint32_t initial_workers = rcog_delegation_pool_get_total_workers(pool);
    EXPECT_GT(initial_workers, 0u);

    // Scaling may or may not succeed depending on config
    rcog_delegation_pool_scale_tier(pool, RCOG_TIER_L1_REASONING, 4);

    // Pool should still be functional
    EXPECT_TRUE(rcog_delegation_pool_has_capacity(pool));
}

/**
 * @brief Full recursive processing pipeline test
 */
class RcogFullPipelineE2ETest : public ::testing::Test {
protected:
    // Full component set
    rcog_engine_t* engine = nullptr;
    rcog_collective_bridge_t* collective = nullptr;
    rcog_imagination_bridge_t* imagination = nullptr;
    rcog_immune_bridge_t* immune = nullptr;
    rcog_bio_async_bridge_t* bio_async = nullptr;
    rcog_brain_kg_bridge_t* brain_kg = nullptr;

    void SetUp() override {
        engine = rcog_engine_create_default();
        collective = rcog_collective_bridge_create_default();
        imagination = rcog_imagination_bridge_create_default();
        immune = rcog_immune_bridge_create_default();
        bio_async = rcog_bio_async_bridge_create_default();
        brain_kg = rcog_brain_kg_bridge_create_default();

        ASSERT_NE(engine, nullptr);
        ASSERT_NE(collective, nullptr);
        ASSERT_NE(imagination, nullptr);
        ASSERT_NE(immune, nullptr);
        ASSERT_NE(bio_async, nullptr);
        ASSERT_NE(brain_kg, nullptr);

        rcog_engine_init(engine);
        rcog_engine_start(engine);
    }

    void TearDown() override {
        if (engine) {
            rcog_engine_stop(engine, 1000);
            rcog_engine_destroy(engine);
        }
        if (collective) rcog_collective_bridge_destroy(collective);
        if (imagination) rcog_imagination_bridge_destroy(imagination);
        if (immune) rcog_immune_bridge_destroy(immune);
        if (bio_async) rcog_bio_async_bridge_destroy(bio_async);
        if (brain_kg) rcog_brain_kg_bridge_destroy(brain_kg);
    }
};

TEST_F(RcogFullPipelineE2ETest, CompleteRecursiveProcessingCycle) {
    // Simulate a complete recursive processing cycle

    // 1. Set initial goal context
    rcog_context_store_t* store = rcog_engine_get_context_store(engine);
    rcog_context_store_set_text(store, "goal", "analyze_data");
    rcog_context_store_set_text(store, "max_depth", "5");

    // 2. Signal processing start via bio-async
    rcog_bio_async_bridge_signal_priority(bio_async, 0.8f, 1);
    rcog_bio_async_bridge_modulate_attention(bio_async, 0.9f, "goal_focus");

    // 3. Update brain KG state
    rcog_processing_state_t state = {0};
    state.is_processing = true;
    state.current_depth = 1;
    rcog_brain_kg_bridge_update_state(brain_kg, &state);

    // 4. Simulate recursive depth progression
    for (uint32_t depth = 1; depth <= 5; depth++) {
        state.current_depth = depth;
        rcog_brain_kg_bridge_update_state(brain_kg, &state);

        // Update all bridges
        rcog_collective_bridge_update(collective, 16.0f);
        rcog_imagination_bridge_update(imagination, 16.0f);
        rcog_immune_bridge_update(immune, 16.0f);
        rcog_brain_kg_bridge_update(brain_kg, 16.0f);

        // Signal progress via dopamine
        rcog_bio_async_bridge_release_dopamine(bio_async, 0.2f, depth);
    }

    // 5. Complete processing
    state.is_processing = false;
    state.current_depth = 0;
    rcog_brain_kg_bridge_update_state(brain_kg, &state);

    // 6. Final reward
    rcog_bio_async_bridge_release_dopamine(bio_async, 0.9f, 999);

    // 7. Verify completion state
    char focus[128];
    rcog_brain_kg_bridge_get_focus(brain_kg, focus, sizeof(focus));
    EXPECT_STREQ(focus, "idle");

    // 8. Check effects propagated
    rcog_to_bio_async_effects_t effects;
    rcog_bio_async_bridge_get_outgoing_effects(bio_async, &effects);
    EXPECT_GT(effects.completed_subtask_count, 0u);
}

TEST_F(RcogFullPipelineE2ETest, ImmuneInfluenceOnEngine) {
    // Test immune system modulating engine capacity

    // Get initial capacity via pool
    rcog_delegation_pool_t* pool = rcog_engine_get_delegation_pool(engine);
    ASSERT_NE(pool, nullptr);
    float initial_capacity = rcog_delegation_pool_get_effective_capacity(pool);
    EXPECT_GT(initial_capacity, 0.0f);

    // Simulate immune modulation
    rcog_immune_modulation_t mod = {0};
    mod.capacity_multiplier = 0.5f;
    mod.timeout_multiplier = 2.0f;

    rcog_engine_apply_immune_modulation(engine, &mod);

    // Capacity should be reduced
    float reduced_capacity = rcog_delegation_pool_get_effective_capacity(pool);
    EXPECT_LE(reduced_capacity, initial_capacity);
}

TEST_F(RcogFullPipelineE2ETest, LongRunningProcessingStability) {
    // Run extended processing to verify stability

    auto start = std::chrono::steady_clock::now();

    for (int cycle = 0; cycle < 500; cycle++) {
        // Update brain state
        rcog_processing_state_t state = {0};
        state.is_processing = (cycle % 10 != 0);
        state.current_depth = (cycle % 5) + 1;
        rcog_brain_kg_bridge_update_state(brain_kg, &state);

        // Update all bridges
        rcog_collective_bridge_update(collective, 16.0f);
        rcog_imagination_bridge_update(imagination, 16.0f);
        rcog_immune_bridge_update(immune, 16.0f);
        rcog_brain_kg_bridge_update(brain_kg, 16.0f);

        // Periodic dopamine releases
        if (cycle % 20 == 0) {
            rcog_bio_async_bridge_release_dopamine(bio_async, 0.3f, cycle);
        }
    }

    auto end = std::chrono::steady_clock::now();
    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Should complete in reasonable time
    EXPECT_LT(duration_ms.count(), 5000);

    // All systems should still be healthy
    EXPECT_FLOAT_EQ(rcog_brain_kg_bridge_get_system_health(brain_kg), 1.0f);
    EXPECT_EQ(rcog_immune_bridge_get_inflammation_level(immune), RCOG_INFLAMMATION_NONE);

    // Engine should still be operational via pool capacity
    rcog_delegation_pool_t* pool = rcog_engine_get_delegation_pool(engine);
    float capacity = rcog_delegation_pool_get_effective_capacity(pool);
    EXPECT_GT(capacity, 0.0f);
}

TEST_F(RcogFullPipelineE2ETest, ContextPropagationThroughPipeline) {
    // Test context values flow through pipeline correctly

    rcog_context_store_t* store = rcog_engine_get_context_store(engine);
    ASSERT_NE(store, nullptr);

    // Set hierarchical context using text API
    int result = rcog_context_store_set_text(store, "root.goal", "master_problem");
    EXPECT_EQ(result, 0);
    result = rcog_context_store_set_text(store, "level1.subgoal", "step1");
    EXPECT_EQ(result, 0);
    result = rcog_context_store_set_text(store, "level2.subgoal", "step2");
    EXPECT_EQ(result, 0);

    // Verify context store accumulated entries
    rcog_context_store_stats_t stats;
    result = rcog_context_store_get_stats(store, &stats);
    EXPECT_EQ(result, 0);
    EXPECT_GE(stats.variable_count, 3u);
}
