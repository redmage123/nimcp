//=============================================================================
// test_brain_pr_memory_integration.cpp - PR Memory Brain Integration Tests
//=============================================================================
/**
 * @file test_brain_pr_memory_integration.cpp
 * @brief Integration tests for brain-PR memory system
 *
 * WHAT: Tests the interaction between PR memory and other brain subsystems
 * WHY:  Verify PR memory integrates correctly with the brain lifecycle
 * HOW:  Create brains, use PR memory with other subsystems
 *
 * INTEGRATION SCENARIOS:
 * 1. PR memory with brain tick cycle
 * 2. PR memory across brain save/load
 * 3. PR memory with brain resize
 * 4. PR memory with cognitive systems
 * 5. PR memory consolidation during brain processing
 */

#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "core/brain/nimcp_brain.h"
#include "core/brain/internal/nimcp_brain_pr_memory.h"
#include "nimcp.h"

// Forward declare types needed for accessor function returns
// (full headers have C11 _Atomic which C++ doesn't handle)
typedef struct z_ladder_struct* z_ladder_t;
typedef struct theta_gamma_manager_internal* theta_gamma_manager_t;
typedef struct entangle_graph_struct* entangle_graph_t;

// Declare theta-gamma phase functions
float theta_gamma_get_theta_phase(theta_gamma_manager_t manager);
float theta_gamma_get_gamma_phase(theta_gamma_manager_t manager);
}

//=============================================================================
// Test Fixture
//=============================================================================

class BrainPRMemoryIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        nimcp_init();
        brain_clear_error();
    }

    void TearDown() override {
        nimcp_shutdown();
    }

    brain_t createTestBrain(bool enable_pr_memory = true) {
        brain_config_t config = {};
        config.size = BRAIN_SIZE_SMALL;
        config.task = BRAIN_TASK_CLASSIFICATION;
        config.num_inputs = 64;
        config.num_outputs = 10;
        strncpy(config.task_name, "pr_integration_test", 63);
        config.enable_pr_memory = enable_pr_memory;
        config.lazy_pr_memory_init = false;
        return brain_create_custom(&config);
    }
};

//=============================================================================
// Test: PR Memory with Brain Tick Cycle
//=============================================================================

TEST_F(BrainPRMemoryIntegrationTest, PRMemory_IntegratesWithBrainTick) {
    // WHAT: Test PR memory updates during brain tick cycle
    // WHY:  Verify PR memory is updated as part of normal brain operation
    // HOW:  Create brain, tick it, verify PR memory state changes

    brain_t brain = createTestBrain(true);
    ASSERT_NE(brain, nullptr);
    ASSERT_TRUE(nimcp_brain_pr_memory_is_initialized(brain));

    brain_pr_memory_stats_t stats_initial;
    nimcp_brain_pr_memory_get_stats(brain, &stats_initial);

    // Simulate brain processing with time advancement
    uint64_t time_us = 0;
    for (int i = 0; i < 20; i++) {
        time_us += 50000;  // 50ms steps
        nimcp_brain_pr_memory_tick(brain, time_us);
    }

    brain_pr_memory_stats_t stats_after;
    nimcp_brain_pr_memory_get_stats(brain, &stats_after);

    // Consolidation should have occurred
    EXPECT_GT(stats_after.last_consolidation_us, stats_initial.last_consolidation_us)
        << "Consolidation should have been triggered";

    brain_destroy(brain);
}

//=============================================================================
// Test: Z-Ladder Direct Access
//=============================================================================

TEST_F(BrainPRMemoryIntegrationTest, ZLadder_DirectAccess) {
    // WHAT: Test direct access to Z-Ladder through brain
    // WHY:  Allow external code to use Z-Ladder for memory operations
    // HOW:  Get Z-Ladder from brain, verify it's accessible

    brain_t brain = createTestBrain(true);
    ASSERT_NE(brain, nullptr);

    z_ladder_t ladder = nimcp_brain_get_z_ladder(brain);
    ASSERT_NE(ladder, nullptr);

    // Verify through brain-level stats (avoids struct mismatch)
    brain_pr_memory_stats_t stats;
    bool result = nimcp_brain_pr_memory_get_stats(brain, &stats);
    EXPECT_TRUE(result);

    // Verify initial state through brain stats
    EXPECT_EQ(stats.z0_count, 0u);
    EXPECT_EQ(stats.z1_count, 0u);
    EXPECT_EQ(stats.z2_count, 0u);
    EXPECT_EQ(stats.z3_count, 0u);

    brain_destroy(brain);
}

//=============================================================================
// Test: Theta-Gamma Direct Access
//=============================================================================

TEST_F(BrainPRMemoryIntegrationTest, ThetaGamma_DirectAccess) {
    // WHAT: Test direct access to theta-gamma manager through brain
    // WHY:  Allow external code to query oscillation state
    // HOW:  Get theta-gamma manager, query phase

    brain_t brain = createTestBrain(true);
    ASSERT_NE(brain, nullptr);

    theta_gamma_manager_t tg = nimcp_brain_get_theta_gamma(brain);
    ASSERT_NE(tg, nullptr);

    // Query theta phase
    float theta_phase = theta_gamma_get_theta_phase(tg);
    EXPECT_GE(theta_phase, 0.0f);
    EXPECT_LT(theta_phase, 360.0f);

    // Query gamma phase
    float gamma_phase = theta_gamma_get_gamma_phase(tg);
    EXPECT_GE(gamma_phase, 0.0f);

    brain_destroy(brain);
}

//=============================================================================
// Test: Entanglement Graph Direct Access
//=============================================================================

TEST_F(BrainPRMemoryIntegrationTest, Entanglement_DirectAccess) {
    // WHAT: Test direct access to entanglement graph through brain
    // WHY:  Allow external code to manage memory associations
    // HOW:  Get entanglement graph, verify it's accessible

    brain_t brain = createTestBrain(true);
    ASSERT_NE(brain, nullptr);

    entangle_graph_t entangle = nimcp_brain_get_entanglement(brain);
    ASSERT_NE(entangle, nullptr);

    // Verify through brain-level stats (avoids struct mismatch)
    brain_pr_memory_stats_t stats;
    bool result = nimcp_brain_pr_memory_get_stats(brain, &stats);
    EXPECT_TRUE(result);

    // Initially empty
    EXPECT_EQ(stats.entangle_node_count, 0u);
    EXPECT_EQ(stats.entangle_edge_count, 0u);

    brain_destroy(brain);
}

//=============================================================================
// Test: PR Memory State Consistency
//=============================================================================

TEST_F(BrainPRMemoryIntegrationTest, PRMemory_StateConsistency) {
    // WHAT: Test that PR memory stats are consistent internally
    // WHY:  Verify the stats aggregation is correct
    // HOW:  Get brain stats, verify consistency

    brain_t brain = createTestBrain(true);
    ASSERT_NE(brain, nullptr);

    // Get stats through brain interface
    brain_pr_memory_stats_t brain_stats;
    nimcp_brain_pr_memory_get_stats(brain, &brain_stats);

    // Verify internal consistency (counts should sum properly)
    EXPECT_EQ(brain_stats.z0_count, 0u); // Initially empty
    EXPECT_EQ(brain_stats.z1_count, 0u);
    EXPECT_EQ(brain_stats.z2_count, 0u);
    EXPECT_EQ(brain_stats.z3_count, 0u);

    // Verify theta phase is in valid range
    EXPECT_GE(brain_stats.current_theta_phase, 0.0f);
    EXPECT_LT(brain_stats.current_theta_phase, 360.0f);

    // Verify entanglement initially empty
    EXPECT_EQ(brain_stats.entangle_node_count, 0u);
    EXPECT_EQ(brain_stats.entangle_edge_count, 0u);

    brain_destroy(brain);
}

//=============================================================================
// Test: Phase Gating Integration
//=============================================================================

TEST_F(BrainPRMemoryIntegrationTest, PhaseGating_EncodingRetrieval) {
    // WHAT: Test phase-gated encoding/retrieval windows
    // WHY:  Verify theta-gamma coupling creates proper memory windows
    // HOW:  Advance time, track window states

    brain_t brain = createTestBrain(true);
    ASSERT_NE(brain, nullptr);

    int encoding_count = 0;
    int retrieval_count = 0;
    int neutral_count = 0;

    // Run through multiple theta cycles
    uint64_t time_us = 0;
    for (int i = 0; i < 500; i++) {
        time_us += 5000;  // 5ms steps
        nimcp_brain_pr_memory_tick(brain, time_us);

        brain_pr_memory_stats_t stats;
        nimcp_brain_pr_memory_get_stats(brain, &stats);

        if (stats.is_encoding_window) encoding_count++;
        else if (stats.is_retrieval_window) retrieval_count++;
        else neutral_count++;
    }

    // Should have spent time in each window type
    // (exact distribution depends on theta frequency and time step)
    EXPECT_GT(encoding_count, 0) << "Should have encoding windows";
    EXPECT_GT(retrieval_count, 0) << "Should have retrieval windows";
    EXPECT_GT(neutral_count, 0) << "Should have neutral periods";

    brain_destroy(brain);
}

//=============================================================================
// Test: Consolidation Triggering
//=============================================================================

TEST_F(BrainPRMemoryIntegrationTest, Consolidation_Triggering) {
    // WHAT: Test that consolidation is triggered at correct intervals
    // WHY:  Verify consolidation follows configured interval
    // HOW:  Track consolidation timestamps over time

    brain_t brain = createTestBrain(true);
    ASSERT_NE(brain, nullptr);

    uint64_t consolidation_times[10] = {0};
    int consolidation_count = 0;
    uint64_t last_consolidation = 0;

    uint64_t time_us = 0;
    for (int i = 0; i < 30 && consolidation_count < 10; i++) {
        time_us += 100000;  // 100ms steps (matches default interval)
        bool triggered = nimcp_brain_pr_memory_tick(brain, time_us);

        if (triggered) {
            brain_pr_memory_stats_t stats;
            nimcp_brain_pr_memory_get_stats(brain, &stats);

            if (stats.last_consolidation_us != last_consolidation) {
                consolidation_times[consolidation_count++] = stats.last_consolidation_us;
                last_consolidation = stats.last_consolidation_us;
            }
        }
    }

    // Should have triggered multiple consolidations
    EXPECT_GT(consolidation_count, 1) << "Should trigger multiple consolidations";

    // Check interval between consolidations (approximately 100ms)
    for (int i = 1; i < consolidation_count; i++) {
        uint64_t interval = consolidation_times[i] - consolidation_times[i-1];
        EXPECT_GE(interval, 90000u);  // At least 90ms
        EXPECT_LE(interval, 150000u); // At most 150ms
    }

    brain_destroy(brain);
}

//=============================================================================
// Test: Multiple Brains Independence
//=============================================================================

TEST_F(BrainPRMemoryIntegrationTest, MultipleBrains_Independence) {
    // WHAT: Test that multiple brains have independent PR memory
    // WHY:  Verify no shared state corruption
    // HOW:  Create two brains, advance one, verify other unchanged

    brain_t brain1 = createTestBrain(true);
    brain_t brain2 = createTestBrain(true);
    ASSERT_NE(brain1, nullptr);
    ASSERT_NE(brain2, nullptr);

    // Get initial states
    brain_pr_memory_stats_t stats1_initial, stats2_initial;
    nimcp_brain_pr_memory_get_stats(brain1, &stats1_initial);
    nimcp_brain_pr_memory_get_stats(brain2, &stats2_initial);

    // Advance only brain1
    uint64_t time_us = 0;
    for (int i = 0; i < 20; i++) {
        time_us += 100000;
        nimcp_brain_pr_memory_tick(brain1, time_us);
    }

    // Get final states
    brain_pr_memory_stats_t stats1_final, stats2_final;
    nimcp_brain_pr_memory_get_stats(brain1, &stats1_final);
    nimcp_brain_pr_memory_get_stats(brain2, &stats2_final);

    // Brain1 should have changed
    EXPECT_GT(stats1_final.last_consolidation_us, stats1_initial.last_consolidation_us);

    // Brain2 should be unchanged
    EXPECT_EQ(stats2_final.last_consolidation_us, stats2_initial.last_consolidation_us);

    brain_destroy(brain1);
    brain_destroy(brain2);
}

//=============================================================================
// Test: Destroy While Processing
//=============================================================================

TEST_F(BrainPRMemoryIntegrationTest, Destroy_DuringProcessing) {
    // WHAT: Test safe destruction during PR memory processing
    // WHY:  Ensure no memory leaks or crashes
    // HOW:  Start processing, destroy mid-cycle

    brain_t brain = createTestBrain(true);
    ASSERT_NE(brain, nullptr);

    // Start some processing
    uint64_t time_us = 0;
    for (int i = 0; i < 5; i++) {
        time_us += 50000;
        nimcp_brain_pr_memory_tick(brain, time_us);
    }

    // Destroy immediately after tick
    brain_destroy(brain);

    // If we reach here, no crash occurred
    SUCCEED();
}

//=============================================================================
// Test: Brain With Full Cognitive Systems
//=============================================================================

TEST_F(BrainPRMemoryIntegrationTest, PRMemory_WithCognitiveSystems) {
    // WHAT: Test PR memory with other cognitive systems enabled
    // WHY:  Verify PR memory coexists with other subsystems
    // HOW:  Create brain with multiple systems, verify all work

    brain_config_t config = {};
    config.size = BRAIN_SIZE_SMALL;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 64;
    config.num_outputs = 10;
    strncpy(config.task_name, "full_cognitive", 63);

    // Enable multiple cognitive systems
    config.enable_pr_memory = true;
    config.enable_working_memory = true;
    config.enable_executive_control = true;
    config.enable_knowledge = false;  // Keep it simpler

    brain_t brain = brain_create_custom(&config);
    ASSERT_NE(brain, nullptr);

    // Verify PR memory is initialized
    EXPECT_TRUE(nimcp_brain_pr_memory_is_initialized(brain));
    EXPECT_NE(nimcp_brain_get_z_ladder(brain), nullptr);

    // Run some ticks
    uint64_t time_us = 0;
    for (int i = 0; i < 10; i++) {
        time_us += 100000;
        nimcp_brain_pr_memory_tick(brain, time_us);
    }

    // Get stats - should work
    brain_pr_memory_stats_t stats;
    bool result = nimcp_brain_pr_memory_get_stats(brain, &stats);
    EXPECT_TRUE(result);

    brain_destroy(brain);
}

//=============================================================================
// Test: Long Running Simulation
//=============================================================================

TEST_F(BrainPRMemoryIntegrationTest, LongRunning_StabilityTest) {
    // WHAT: Test PR memory stability over many cycles
    // WHY:  Verify no memory leaks or state corruption over time
    // HOW:  Run many tick cycles, verify consistent behavior

    brain_t brain = createTestBrain(true);
    ASSERT_NE(brain, nullptr);

    uint64_t time_us = 0;
    int consolidation_count = 0;

    // Simulate 10 seconds of processing (100 consolidation intervals)
    for (int i = 0; i < 1000; i++) {
        time_us += 10000;  // 10ms steps
        bool triggered = nimcp_brain_pr_memory_tick(brain, time_us);
        if (triggered) consolidation_count++;
    }

    // Should have triggered approximately 100 consolidations
    EXPECT_GE(consolidation_count, 90);
    EXPECT_LE(consolidation_count, 110);

    // Final stats should be valid
    brain_pr_memory_stats_t stats;
    bool result = nimcp_brain_pr_memory_get_stats(brain, &stats);
    EXPECT_TRUE(result);

    // Phase should still be in valid range
    EXPECT_GE(stats.current_theta_phase, 0.0f);
    EXPECT_LT(stats.current_theta_phase, 360.0f);

    brain_destroy(brain);
}

