//=============================================================================
// e2e_test_brain_pr_memory_pipeline.cpp - PR Memory Brain E2E Tests
//=============================================================================
/**
 * @file e2e_test_brain_pr_memory_pipeline.cpp
 * @brief E2E Tests for Prime Resonant Memory Brain Integration
 *
 * WHAT: Complete PR memory pipeline tests with brain lifecycle
 * WHY:  Verify PR memory system works correctly across full brain operation
 * HOW:  Test complete scenarios from brain creation through processing
 *
 * TEST PIPELINES:
 * 1. BrainCreationPipeline: Brain creation with PR memory initialization
 * 2. ConsolidationPipeline: Memory consolidation during brain operation
 * 3. PhaseGatingPipeline: Theta-gamma phase gating throughout lifecycle
 * 4. MultiRegionPipeline: PR memory with multiple brain regions
 *
 * @author NIMCP Development Team
 * @date 2026-01-10
 * @version 1.0.0
 */

#include <gtest/gtest.h>
#include <chrono>
#include <thread>
#include <cstring>

extern "C" {
#include "core/brain/nimcp_brain.h"
#include "core/brain/internal/nimcp_brain_pr_memory.h"
#include "nimcp.h"

// Forward declare types needed for function parameters
// (full headers have C11 _Atomic which C++ doesn't handle)
typedef struct z_ladder_struct* z_ladder_t;
typedef struct theta_gamma_manager_internal* theta_gamma_manager_t;
typedef struct entangle_graph_struct* entangle_graph_t;
}

//=============================================================================
// Test Fixture
//=============================================================================

class BrainPRMemoryE2ETest : public ::testing::Test {
protected:
    void SetUp() override {
        nimcp_init();
        brain_clear_error();
    }

    void TearDown() override {
        nimcp_shutdown();
    }

    // Create a fully configured brain for e2e testing
    brain_t createFullBrain() {
        brain_config_t config = {};
        config.size = BRAIN_SIZE_MEDIUM;
        config.task = BRAIN_TASK_CLASSIFICATION;
        config.num_inputs = 128;
        config.num_outputs = 20;
        strncpy(config.task_name, "pr_e2e_test", 63);

        // Enable PR memory
        config.enable_pr_memory = true;
        config.lazy_pr_memory_init = false;

        // Enable other cognitive systems for full integration
        config.enable_working_memory = true;
        config.enable_executive_control = true;

        return brain_create_custom(&config);
    }
};

//=============================================================================
// E2E Test: Brain Creation Pipeline with PR Memory
//=============================================================================

TEST_F(BrainPRMemoryE2ETest, Pipeline_BrainCreationWithPRMemory) {
    // WHAT: Test complete brain creation with PR memory
    // WHY:  Verify PR memory integrates correctly during brain initialization
    // HOW:  Create brain, verify all PR components are operational

    // Step 1: Create brain with full PR memory configuration
    brain_t brain = createFullBrain();
    ASSERT_NE(brain, nullptr) << "Brain creation failed";

    // Step 2: Verify PR memory is initialized
    EXPECT_TRUE(nimcp_brain_pr_memory_is_initialized(brain))
        << "PR memory should be initialized";

    // Step 3: Verify all three PR subsystems
    z_ladder_t z_ladder = nimcp_brain_get_z_ladder(brain);
    ASSERT_NE(z_ladder, nullptr) << "Z-Ladder not available";

    theta_gamma_manager_t tg = nimcp_brain_get_theta_gamma(brain);
    ASSERT_NE(tg, nullptr) << "Theta-gamma not available";

    entangle_graph_t entangle = nimcp_brain_get_entanglement(brain);
    ASSERT_NE(entangle, nullptr) << "Entanglement not available";

    // Step 4: Get initial stats
    brain_pr_memory_stats_t stats;
    bool stats_ok = nimcp_brain_pr_memory_get_stats(brain, &stats);
    ASSERT_TRUE(stats_ok) << "Failed to get PR memory stats";

    // Step 5: Verify initial state
    EXPECT_EQ(stats.z0_count, 0u) << "Z0 should be empty initially";
    EXPECT_EQ(stats.z1_count, 0u) << "Z1 should be empty initially";
    EXPECT_EQ(stats.z2_count, 0u) << "Z2 should be empty initially";
    EXPECT_EQ(stats.z3_count, 0u) << "Z3 should be empty initially";
    EXPECT_EQ(stats.entangle_node_count, 0u) << "Entanglement should be empty";

    // Cleanup
    brain_destroy(brain);
}

//=============================================================================
// E2E Test: Consolidation Pipeline
//=============================================================================

TEST_F(BrainPRMemoryE2ETest, Pipeline_ConsolidationCycle) {
    // WHAT: Test memory consolidation over extended operation
    // WHY:  Verify consolidation works correctly during brain processing
    // HOW:  Run brain for simulated time, verify consolidation triggers

    brain_t brain = createFullBrain();
    ASSERT_NE(brain, nullptr);

    // Track consolidation events
    uint64_t first_consolidation_us = 0;
    int consolidation_count = 0;
    uint64_t last_consolidation_us = 0;

    // Simulate 5 seconds of brain operation (50 consolidation cycles at 100ms)
    uint64_t time_us = 0;
    const uint64_t end_time_us = 5000000;  // 5 seconds
    const uint64_t step_us = 10000;        // 10ms steps

    while (time_us < end_time_us) {
        time_us += step_us;
        bool triggered = nimcp_brain_pr_memory_tick(brain, time_us);

        if (triggered) {
            brain_pr_memory_stats_t stats;
            nimcp_brain_pr_memory_get_stats(brain, &stats);

            if (first_consolidation_us == 0) {
                first_consolidation_us = stats.last_consolidation_us;
            }

            if (stats.last_consolidation_us != last_consolidation_us) {
                consolidation_count++;
                last_consolidation_us = stats.last_consolidation_us;
            }
        }
    }

    // Verify consolidation happened
    EXPECT_GT(consolidation_count, 0) << "No consolidations triggered";

    // Should have approximately 50 consolidations (5s / 100ms)
    EXPECT_GE(consolidation_count, 40) << "Too few consolidations";
    EXPECT_LE(consolidation_count, 60) << "Too many consolidations";

    brain_destroy(brain);
}

//=============================================================================
// E2E Test: Phase Gating Pipeline
//=============================================================================

TEST_F(BrainPRMemoryE2ETest, Pipeline_PhaseGatingComplete) {
    // WHAT: Test theta-gamma phase gating over complete cycles
    // WHY:  Verify biologically-plausible encoding/retrieval windows
    // HOW:  Run through multiple theta cycles, verify window distribution

    brain_t brain = createFullBrain();
    ASSERT_NE(brain, nullptr);

    // Track phase windows
    int encoding_windows = 0;
    int retrieval_windows = 0;
    int neutral_periods = 0;

    // Track phase distribution
    float min_phase = 360.0f;
    float max_phase = 0.0f;

    // Run for several complete theta cycles (6 Hz = ~167ms per cycle)
    // Run for 2 seconds to get ~12 complete cycles
    uint64_t time_us = 0;
    const uint64_t end_time_us = 2000000;  // 2 seconds
    const uint64_t step_us = 5000;         // 5ms steps

    while (time_us < end_time_us) {
        time_us += step_us;
        nimcp_brain_pr_memory_tick(brain, time_us);

        brain_pr_memory_stats_t stats;
        nimcp_brain_pr_memory_get_stats(brain, &stats);

        // Track phase range
        if (stats.current_theta_phase < min_phase)
            min_phase = stats.current_theta_phase;
        if (stats.current_theta_phase > max_phase)
            max_phase = stats.current_theta_phase;

        // Count window types
        if (stats.is_encoding_window) {
            encoding_windows++;
        } else if (stats.is_retrieval_window) {
            retrieval_windows++;
        } else {
            neutral_periods++;
        }
    }

    // Verify phase covered full range
    EXPECT_LT(min_phase, 10.0f) << "Should have reached near 0°";
    EXPECT_GT(max_phase, 350.0f) << "Should have reached near 360°";

    // Verify all window types occurred
    EXPECT_GT(encoding_windows, 0) << "No encoding windows detected";
    EXPECT_GT(retrieval_windows, 0) << "No retrieval windows detected";
    EXPECT_GT(neutral_periods, 0) << "No neutral periods detected";

    // Verify approximate distribution (encoding + retrieval should be ~50% of time)
    int total_samples = encoding_windows + retrieval_windows + neutral_periods;
    float encoding_ratio = (float)encoding_windows / total_samples;
    float retrieval_ratio = (float)retrieval_windows / total_samples;

    // Each window (0-90° and 180-270°) should be ~25% of total
    EXPECT_GE(encoding_ratio, 0.15f) << "Encoding ratio too low";
    EXPECT_LE(encoding_ratio, 0.35f) << "Encoding ratio too high";
    EXPECT_GE(retrieval_ratio, 0.15f) << "Retrieval ratio too low";
    EXPECT_LE(retrieval_ratio, 0.35f) << "Retrieval ratio too high";

    brain_destroy(brain);
}

//=============================================================================
// E2E Test: Extended Operation Pipeline
//=============================================================================

TEST_F(BrainPRMemoryE2ETest, Pipeline_ExtendedOperation) {
    // WHAT: Test PR memory stability over extended operation
    // WHY:  Verify no degradation or resource exhaustion over time
    // HOW:  Run brain for simulated long period, check stability

    brain_t brain = createFullBrain();
    ASSERT_NE(brain, nullptr);

    // Simulate 30 seconds of operation
    uint64_t time_us = 0;
    const uint64_t end_time_us = 30000000;  // 30 seconds
    const uint64_t step_us = 10000;         // 10ms steps

    int tick_count = 0;
    int consolidation_count = 0;

    while (time_us < end_time_us) {
        time_us += step_us;
        bool triggered = nimcp_brain_pr_memory_tick(brain, time_us);

        tick_count++;
        if (triggered) consolidation_count++;

        // Periodically check stats are still valid
        if (tick_count % 100 == 0) {
            brain_pr_memory_stats_t stats;
            bool ok = nimcp_brain_pr_memory_get_stats(brain, &stats);
            EXPECT_TRUE(ok) << "Stats retrieval failed at tick " << tick_count;

            // Phase should always be in valid range
            EXPECT_GE(stats.current_theta_phase, 0.0f);
            EXPECT_LT(stats.current_theta_phase, 360.0f);
        }
    }

    // Final verification
    brain_pr_memory_stats_t final_stats;
    ASSERT_TRUE(nimcp_brain_pr_memory_get_stats(brain, &final_stats));

    // Should have had many consolidations (~300 over 30 seconds)
    EXPECT_GE(consolidation_count, 250) << "Expected more consolidations";

    brain_destroy(brain);
}

//=============================================================================
// E2E Test: Multiple Brains Pipeline
//=============================================================================

TEST_F(BrainPRMemoryE2ETest, Pipeline_MultipleBrains) {
    // WHAT: Test multiple brains with independent PR memory
    // WHY:  Verify no shared state corruption between brains
    // HOW:  Create multiple brains, run them concurrently

    const int NUM_BRAINS = 3;
    brain_t brains[NUM_BRAINS];

    // Create multiple brains
    for (int i = 0; i < NUM_BRAINS; i++) {
        brains[i] = createFullBrain();
        ASSERT_NE(brains[i], nullptr) << "Failed to create brain " << i;
        ASSERT_TRUE(nimcp_brain_pr_memory_is_initialized(brains[i]))
            << "PR memory not initialized for brain " << i;
    }

    // Run them at different rates (simulating different processing speeds)
    uint64_t times_us[NUM_BRAINS] = {0, 0, 0};
    const uint64_t steps_us[NUM_BRAINS] = {10000, 15000, 20000};  // Different rates
    const uint64_t end_time_us = 1000000;  // 1 second

    while (times_us[0] < end_time_us) {
        for (int i = 0; i < NUM_BRAINS; i++) {
            times_us[i] += steps_us[i];
            nimcp_brain_pr_memory_tick(brains[i], times_us[i]);
        }
    }

    // Verify each brain has independent state
    brain_pr_memory_stats_t stats[NUM_BRAINS];
    for (int i = 0; i < NUM_BRAINS; i++) {
        ASSERT_TRUE(nimcp_brain_pr_memory_get_stats(brains[i], &stats[i]));
    }

    // Last consolidation times should be different (different rates)
    EXPECT_NE(stats[0].last_consolidation_us, stats[1].last_consolidation_us)
        << "Brains 0 and 1 should have different consolidation times";
    EXPECT_NE(stats[1].last_consolidation_us, stats[2].last_consolidation_us)
        << "Brains 1 and 2 should have different consolidation times";

    // Z-Ladders should be different pointers
    for (int i = 0; i < NUM_BRAINS; i++) {
        for (int j = i + 1; j < NUM_BRAINS; j++) {
            EXPECT_NE(nimcp_brain_get_z_ladder(brains[i]),
                      nimcp_brain_get_z_ladder(brains[j]))
                << "Brains " << i << " and " << j << " share Z-Ladder";
        }
    }

    // Cleanup
    for (int i = 0; i < NUM_BRAINS; i++) {
        brain_destroy(brains[i]);
    }
}

//=============================================================================
// E2E Test: Brain Lifecycle Pipeline
//=============================================================================

TEST_F(BrainPRMemoryE2ETest, Pipeline_CompleteLifecycle) {
    // WHAT: Test complete brain lifecycle with PR memory
    // WHY:  Verify PR memory works through creation, operation, destruction
    // HOW:  Run complete lifecycle multiple times

    const int NUM_CYCLES = 5;

    for (int cycle = 0; cycle < NUM_CYCLES; cycle++) {
        // Create
        brain_t brain = createFullBrain();
        ASSERT_NE(brain, nullptr) << "Creation failed on cycle " << cycle;

        // Verify initialization
        EXPECT_TRUE(nimcp_brain_pr_memory_is_initialized(brain))
            << "PR memory not initialized on cycle " << cycle;

        // Operate
        uint64_t time_us = 0;
        for (int i = 0; i < 100; i++) {
            time_us += 50000;  // 50ms steps
            nimcp_brain_pr_memory_tick(brain, time_us);
        }

        // Verify operation
        brain_pr_memory_stats_t stats;
        EXPECT_TRUE(nimcp_brain_pr_memory_get_stats(brain, &stats))
            << "Stats failed on cycle " << cycle;
        EXPECT_GT(stats.last_consolidation_us, 0u)
            << "No consolidation on cycle " << cycle;

        // Destroy
        brain_destroy(brain);
    }
}

//=============================================================================
// E2E Test: Stress Test Pipeline
//=============================================================================

TEST_F(BrainPRMemoryE2ETest, Pipeline_StressTest) {
    // WHAT: Stress test PR memory under heavy load
    // WHY:  Verify robustness under demanding conditions
    // HOW:  Rapid tick calls and frequent stats queries

    brain_t brain = createFullBrain();
    ASSERT_NE(brain, nullptr);

    // Very rapid ticks with frequent stats queries
    uint64_t time_us = 0;
    const int NUM_ITERATIONS = 10000;

    for (int i = 0; i < NUM_ITERATIONS; i++) {
        time_us += 1000;  // 1ms steps (very rapid)
        nimcp_brain_pr_memory_tick(brain, time_us);

        // Query stats every 10 iterations
        if (i % 10 == 0) {
            brain_pr_memory_stats_t stats;
            bool ok = nimcp_brain_pr_memory_get_stats(brain, &stats);
            EXPECT_TRUE(ok) << "Stats query failed at iteration " << i;
        }
    }

    // Final check
    brain_pr_memory_stats_t final_stats;
    ASSERT_TRUE(nimcp_brain_pr_memory_get_stats(brain, &final_stats));

    // Should have completed without issues
    EXPECT_GT(final_stats.last_consolidation_us, 0u);

    brain_destroy(brain);
}

//=============================================================================
// E2E Test: Lazy Init Then Full Operation
//=============================================================================

TEST_F(BrainPRMemoryE2ETest, Pipeline_LazyInitThenOperate) {
    // WHAT: Test lazy initialization followed by full operation
    // WHY:  Verify deferred init works correctly when triggered later
    // HOW:  Create with lazy init, then manually init and operate

    // Create with lazy init
    brain_config_t config = {};
    config.size = BRAIN_SIZE_SMALL;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 64;
    config.num_outputs = 10;
    strncpy(config.task_name, "lazy_e2e_test", 63);
    config.enable_pr_memory = true;
    config.lazy_pr_memory_init = true;

    brain_t brain = brain_create_custom(&config);
    ASSERT_NE(brain, nullptr);

    // Verify not initialized
    EXPECT_FALSE(nimcp_brain_pr_memory_is_initialized(brain));

    // Tick should be no-op
    EXPECT_FALSE(nimcp_brain_pr_memory_tick(brain, 100000));

    // Manually initialize
    ASSERT_TRUE(nimcp_brain_pr_memory_init(brain, NULL));

    // Now should be initialized
    EXPECT_TRUE(nimcp_brain_pr_memory_is_initialized(brain));

    // Now operation should work
    uint64_t time_us = 0;
    int consolidation_count = 0;

    for (int i = 0; i < 50; i++) {
        time_us += 100000;
        if (nimcp_brain_pr_memory_tick(brain, time_us)) {
            consolidation_count++;
        }
    }

    EXPECT_GT(consolidation_count, 0) << "No consolidations after manual init";

    brain_destroy(brain);
}

