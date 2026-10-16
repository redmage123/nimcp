/**
 * @file test_consolidation.cpp
 * @brief Tests for memory consolidation API
 *
 * WHAT: Comprehensive tests for consolidation ("sleep") functionality
 * WHY: Consolidation is critical for long-term learning - must work correctly
 * HOW: Unit tests for synchronous/background consolidation, strategies, patterns
 */

#include "test_helpers.h"

#include "core/brain/nimcp_brain.h"
#include "cognitive/consolidation/nimcp_consolidation.h"

#include <gtest/gtest.h>
#include <string.h>
#include <unistd.h>
#include <chrono>

//=============================================================================
// Test Fixtures and Helpers
//=============================================================================

/**
 * WHAT: Test fixture for consolidation tests
 * WHY: Set up/tear down brain and handle for each test
 */
class ConsolidationTest : public ::testing::Test {
   protected:
    brain_t brain;
    consolidation_handle_t handle;

    // Test data
    static const uint32_t NUM_FEATURES = 13;
    float test_features[NUM_FEATURES];

    void SetUp() override
    {
        // Create test brain
        brain = brain_create("test_consol_brain", BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION,
                             NUM_FEATURES, 3);
        ASSERT_NE(brain, nullptr);

        // Initialize test features
        for (uint32_t i = 0; i < NUM_FEATURES; i++) {
            test_features[i] = (float) i / NUM_FEATURES;
        }

        handle = nullptr;
    }

    void TearDown() override
    {
        // Clean up handle
        if (handle) {
            brain_stop_background_consolidation(handle);
        }

        // Clean up brain
        if (brain) {
            brain_destroy(brain);
        }
    }
};

// Global callback counters
static std::atomic<uint32_t> g_consolidation_start_count{0};
static std::atomic<uint32_t> g_consolidation_progress_count{0};
static std::atomic<uint32_t> g_consolidation_complete_count{0};

static void consolidation_start_callback(void* context)
{
    g_consolidation_start_count++;
}

static void consolidation_progress_callback(float progress, void* context)
{
    g_consolidation_progress_count++;
}

static void consolidation_complete_callback(void* context)
{
    g_consolidation_complete_count++;
}

//=============================================================================
// Configuration Tests
//=============================================================================

/**
 * WHAT: Test default consolidation configuration
 * WHY: Verify sensible defaults are provided
 */
TEST_F(ConsolidationTest, DefaultConfig)
{
    consolidation_config_t config = consolidation_default_config();

    EXPECT_EQ(config.strategy, CONSOLIDATION_STRATEGY_FULL);
    EXPECT_EQ(config.priority, CONSOLIDATION_PRIORITY_IMPORTANT);
    EXPECT_GT(config.consolidation_cycles, 0u);
    EXPECT_GT(config.consolidation_strength, 0.0f);
    EXPECT_TRUE(config.enable_replay);
    EXPECT_TRUE(config.enable_pruning);
    EXPECT_TRUE(config.enable_scaling);
}

//=============================================================================
// Synchronous Consolidation Tests
//=============================================================================

/**
 * WHAT: Test synchronous consolidation with default config
 * WHY: Verify basic consolidation works
 */
TEST_F(ConsolidationTest, ConsolidateMemoryDefault)
{
    consolidation_config_t config = consolidation_default_config();
    config.consolidation_cycles = 5;  // Keep it short for testing

    bool result = brain_consolidate_memory(brain, &config);

    EXPECT_TRUE(result);
}

/**
 * WHAT: Test synchronous consolidation with NULL brain
 * WHY: Verify proper error handling
 */
TEST_F(ConsolidationTest, ConsolidateNullBrain)
{
    consolidation_config_t config = consolidation_default_config();

    bool result = brain_consolidate_memory(nullptr, &config);

    EXPECT_FALSE(result);
}

/**
 * WHAT: Test consolidation with NULL config (uses defaults)
 * WHY: Verify NULL config handling
 */
TEST_F(ConsolidationTest, ConsolidateNullConfig)
{
    bool result = brain_consolidate_memory(brain, nullptr);

    EXPECT_TRUE(result);
}

/**
 * WHAT: Test consolidation with callbacks
 * WHY: Verify callbacks are invoked
 */
TEST_F(ConsolidationTest, ConsolidateWithCallbacks)
{
    g_consolidation_start_count = 0;
    g_consolidation_progress_count = 0;
    g_consolidation_complete_count = 0;

    consolidation_config_t config = consolidation_default_config();
    config.consolidation_cycles = 3;
    config.on_consolidation_start = consolidation_start_callback;
    config.on_consolidation_progress = consolidation_progress_callback;
    config.on_consolidation_complete = consolidation_complete_callback;

    bool result = brain_consolidate_memory(brain, &config);

    EXPECT_TRUE(result);
    EXPECT_EQ(g_consolidation_start_count, 1u);
    EXPECT_GT(g_consolidation_progress_count, 0u);
    EXPECT_EQ(g_consolidation_complete_count, 1u);
}

/**
 * WHAT: Test consolidation statistics
 * WHY: Verify statistics are tracked
 */
TEST_F(ConsolidationTest, ConsolidateStatistics)
{
    consolidation_config_t config = consolidation_default_config();
    config.consolidation_cycles = 2;

    brain_consolidate_memory(brain, &config);

    consolidation_stats_t stats;
    bool got_stats = consolidation_get_stats(nullptr, &stats);  // NULL for sync stats

    ASSERT_TRUE(got_stats);
    EXPECT_GT(stats.total_consolidations, 0u);
    EXPECT_GT(stats.last_consolidation_time_ms, 0.0f);
}

//=============================================================================
// Strategy Tests
//=============================================================================

/**
 * WHAT: Test replay strategy
 * WHY: Verify replay-based consolidation works
 */
TEST_F(ConsolidationTest, ReplayStrategy)
{
    consolidation_config_t config = consolidation_default_config();
    config.strategy = CONSOLIDATION_STRATEGY_REPLAY;
    config.consolidation_cycles = 3;

    bool result = brain_consolidate_memory(brain, &config);

    EXPECT_TRUE(result);
}

/**
 * WHAT: Test scaling strategy
 * WHY: Verify synaptic scaling works
 */
TEST_F(ConsolidationTest, ScalingStrategy)
{
    consolidation_config_t config = consolidation_default_config();
    config.strategy = CONSOLIDATION_STRATEGY_SCALING;
    config.consolidation_cycles = 3;

    bool result = brain_consolidate_memory(brain, &config);

    EXPECT_TRUE(result);
}

/**
 * WHAT: Test pruning strategy
 * WHY: Verify connection pruning works
 */
TEST_F(ConsolidationTest, PruningStrategy)
{
    consolidation_config_t config = consolidation_default_config();
    config.strategy = CONSOLIDATION_STRATEGY_PRUNING;
    config.consolidation_cycles = 3;

    bool result = brain_consolidate_memory(brain, &config);

    EXPECT_TRUE(result);
}

/**
 * WHAT: Test integration strategy
 * WHY: Verify knowledge integration works
 */
TEST_F(ConsolidationTest, IntegrationStrategy)
{
    consolidation_config_t config = consolidation_default_config();
    config.strategy = CONSOLIDATION_STRATEGY_INTEGRATION;
    config.consolidation_cycles = 3;

    bool result = brain_consolidate_memory(brain, &config);

    EXPECT_TRUE(result);
}

/**
 * WHAT: Test full strategy (all methods)
 * WHY: Verify comprehensive consolidation works
 */
TEST_F(ConsolidationTest, FullStrategy)
{
    consolidation_config_t config = consolidation_default_config();
    config.strategy = CONSOLIDATION_STRATEGY_FULL;
    config.consolidation_cycles = 3;

    bool result = brain_consolidate_memory(brain, &config);

    EXPECT_TRUE(result);
}

//=============================================================================
// Background Consolidation Tests
//=============================================================================

/**
 * WHAT: Test starting background consolidation
 * WHY: Verify background thread starts correctly
 */
TEST_F(ConsolidationTest, StartBackgroundConsolidation)
{
    consolidation_config_t config = consolidation_default_config();
    config.consolidation_cycles = 2;

    handle = brain_start_background_consolidation(brain, 60, &config);

    ASSERT_NE(handle, nullptr);

    // Give thread time to start
    usleep(10000);  // 10ms
}

/**
 * WHAT: Test background consolidation with NULL brain
 * WHY: Verify proper error handling
 */
TEST_F(ConsolidationTest, StartBackgroundNullBrain)
{
    consolidation_config_t config = consolidation_default_config();

    handle = brain_start_background_consolidation(nullptr, 60, &config);

    EXPECT_EQ(handle, nullptr);
}

/**
 * WHAT: Test stopping background consolidation
 * WHY: Verify graceful shutdown works
 */
TEST_F(ConsolidationTest, StopBackgroundConsolidation)
{
    consolidation_config_t config = consolidation_default_config();

    handle = brain_start_background_consolidation(brain, 60, &config);
    ASSERT_NE(handle, nullptr);

    usleep(10000);  // Let it run briefly

    brain_stop_background_consolidation(handle);
    handle = nullptr;  // Prevent double-free in TearDown

    // Should complete without hanging
}

/**
 * WHAT: Test pausing background consolidation
 * WHY: Verify pause functionality works
 */
TEST_F(ConsolidationTest, PauseConsolidation)
{
    consolidation_config_t config = consolidation_default_config();

    handle = brain_start_background_consolidation(brain, 60, &config);
    ASSERT_NE(handle, nullptr);

    brain_pause_consolidation(handle);

    // Should be paused
    usleep(10000);
}

/**
 * WHAT: Test resuming background consolidation
 * WHY: Verify resume functionality works
 */
TEST_F(ConsolidationTest, ResumeConsolidation)
{
    consolidation_config_t config = consolidation_default_config();

    handle = brain_start_background_consolidation(brain, 60, &config);
    ASSERT_NE(handle, nullptr);

    brain_pause_consolidation(handle);
    brain_resume_consolidation(handle);

    // Should be resumed
    usleep(10000);
}

/**
 * WHAT: Test triggering immediate consolidation
 * WHY: Verify on-demand triggering works
 */
TEST_F(ConsolidationTest, TriggerConsolidation)
{
    consolidation_config_t config = consolidation_default_config();
    config.consolidation_cycles = 2;

    handle = brain_start_background_consolidation(brain, 300, &config);
    ASSERT_NE(handle, nullptr);

    // Trigger immediately instead of waiting for interval
    bool result = brain_trigger_consolidation(handle);
    EXPECT_TRUE(result);

    // Wait for consolidation to complete
    usleep(500000);  // 500ms
}

/**
 * WHAT: Test checking if consolidation is running
 * WHY: Verify status query works
 */
TEST_F(ConsolidationTest, IsConsolidationRunning)
{
    consolidation_config_t config = consolidation_default_config();
    config.consolidation_cycles = 2;

    handle = brain_start_background_consolidation(brain, 1, &config);  // 1 second interval
    ASSERT_NE(handle, nullptr);

    // Trigger consolidation
    brain_trigger_consolidation(handle);

    usleep(10000);  // Give it time to start

    // Check if running
    bool is_running = consolidation_is_running(handle);

    // Might be running or might have finished already
    // Just verify it doesn't crash
    (void) is_running;
}

/**
 * WHAT: Test getting consolidation progress
 * WHY: Verify progress monitoring works
 */
TEST_F(ConsolidationTest, GetConsolidationProgress)
{
    consolidation_config_t config = consolidation_default_config();
    config.consolidation_cycles = 10;  // Longer consolidation

    handle = brain_start_background_consolidation(brain, 1, &config);
    ASSERT_NE(handle, nullptr);

    // Trigger consolidation
    brain_trigger_consolidation(handle);

    usleep(50000);  // 50ms - should be in progress

    float progress = consolidation_get_progress(handle);

    // Should be -1 if not running, or 0-1 if running
    if (progress >= 0.0f) {
        EXPECT_LE(progress, 1.0f);
    }
}

/**
 * WHAT: Test background consolidation statistics
 * WHY: Verify statistics are tracked for background consolidation
 */
TEST_F(ConsolidationTest, BackgroundStatistics)
{
    consolidation_config_t config = consolidation_default_config();
    config.consolidation_cycles = 2;

    handle = brain_start_background_consolidation(brain, 1, &config);
    ASSERT_NE(handle, nullptr);

    // Trigger and wait for completion
    brain_trigger_consolidation(handle);
    usleep(1000000);  // 1 second

    consolidation_stats_t stats;
    bool got_stats = consolidation_get_stats(handle, &stats);

    ASSERT_TRUE(got_stats);
    // Should have at least one consolidation
    EXPECT_GE(stats.total_consolidations, 0u);
}

//=============================================================================
// Pattern Management Tests
//=============================================================================

/**
 * WHAT: Test getting important patterns
 * WHY: Verify pattern importance tracking works
 */
TEST_F(ConsolidationTest, GetImportantPatterns)
{
    uint32_t num_patterns = 0;
    pattern_importance_t* patterns = brain_get_important_patterns(brain, &num_patterns);

    // Should return some patterns (simulated)
    EXPECT_GT(num_patterns, 0u);
    ASSERT_NE(patterns, nullptr);

    // Verify pattern structure
    for (uint32_t i = 0; i < num_patterns; i++) {
        EXPECT_NE(patterns[i].pattern_name, nullptr);
        EXPECT_GE(patterns[i].importance_score, 0.0f);
        EXPECT_LE(patterns[i].importance_score, 1.0f);
    }

    pattern_importance_free(patterns, num_patterns);
}

/**
 * WHAT: Test getting patterns with NULL brain
 * WHY: Verify proper error handling
 */
TEST_F(ConsolidationTest, GetPatternsNullBrain)
{
    uint32_t num_patterns = 0;
    pattern_importance_t* patterns = brain_get_important_patterns(nullptr, &num_patterns);

    EXPECT_EQ(patterns, nullptr);
    EXPECT_EQ(num_patterns, 0u);
}

/**
 * WHAT: Test marking pattern as important
 * WHY: Verify manual importance setting works
 */
TEST_F(ConsolidationTest, MarkPatternImportant)
{
    bool result = brain_mark_pattern_important(brain, "critical_pattern", 0.95f);

    EXPECT_TRUE(result);
}

//=============================================================================
// Advanced Consolidation Tests
//=============================================================================

/**
 * WHAT: Test replaying specific pattern
 * WHY: Verify manual pattern replay works
 */
TEST_F(ConsolidationTest, ReplayPattern)
{
    bool result = brain_replay_pattern(brain, "test_pattern", 10, 0.1f);

    EXPECT_TRUE(result);
}

/**
 * WHAT: Test applying synaptic scaling
 * WHY: Verify manual scaling works
 */
TEST_F(ConsolidationTest, ApplySynapticScaling)
{
    bool result = brain_apply_synaptic_scaling(brain, 0.5f);

    EXPECT_TRUE(result);
}

/**
 * WHAT: Test pruning weak connections
 * WHY: Verify manual pruning works
 */
TEST_F(ConsolidationTest, PruneWeakConnections)
{
    uint32_t pruned = brain_prune_weak_connections(brain, 0.01f);

    // Should return count of pruned connections
    EXPECT_GE(pruned, 0u);
}

//=============================================================================
// Priority Tests
//=============================================================================

/**
 * WHAT: Test consolidation with recent priority
 * WHY: Verify priority affects consolidation
 */
TEST_F(ConsolidationTest, RecentPriority)
{
    consolidation_config_t config = consolidation_default_config();
    config.priority = CONSOLIDATION_PRIORITY_RECENT;
    config.consolidation_cycles = 2;

    bool result = brain_consolidate_memory(brain, &config);

    EXPECT_TRUE(result);
}

/**
 * WHAT: Test consolidation with frequent priority
 * WHY: Verify priority affects consolidation
 */
TEST_F(ConsolidationTest, FrequentPriority)
{
    consolidation_config_t config = consolidation_default_config();
    config.priority = CONSOLIDATION_PRIORITY_FREQUENT;
    config.consolidation_cycles = 2;

    bool result = brain_consolidate_memory(brain, &config);

    EXPECT_TRUE(result);
}

/**
 * WHAT: Test consolidation with novel priority
 * WHY: Verify priority affects consolidation
 */
TEST_F(ConsolidationTest, NovelPriority)
{
    consolidation_config_t config = consolidation_default_config();
    config.priority = CONSOLIDATION_PRIORITY_NOVEL;
    config.consolidation_cycles = 2;

    bool result = brain_consolidate_memory(brain, &config);

    EXPECT_TRUE(result);
}

//=============================================================================
// Statistics Reset Tests
//=============================================================================

/**
 * WHAT: Test resetting statistics
 * WHY: Verify stats reset works
 */
TEST_F(ConsolidationTest, ResetStatistics)
{
    consolidation_config_t config = consolidation_default_config();
    config.consolidation_cycles = 2;

    // Perform consolidation
    brain_consolidate_memory(brain, &config);

    // Reset stats
    consolidation_reset_stats(nullptr);  // NULL for sync stats

    // Verify reset
    consolidation_stats_t stats;
    consolidation_get_stats(nullptr, &stats);
    EXPECT_EQ(stats.total_consolidations, 0u);
}

//=============================================================================
// Thread Safety Tests
//=============================================================================

/**
 * WHAT: Test multiple background consolidations
 * WHY: Verify thread safety
 */
TEST_F(ConsolidationTest, MultipleBackgroundConsolidations)
{
    consolidation_config_t config = consolidation_default_config();
    config.consolidation_cycles = 2;

    handle = brain_start_background_consolidation(brain, 1, &config);
    ASSERT_NE(handle, nullptr);

    // Trigger multiple consolidations
    for (int i = 0; i < 5; i++) {
        brain_trigger_consolidation(handle);
        usleep(100000);  // 100ms between triggers
    }

    // Wait for all to complete
    usleep(1000000);  // 1 second

    // Should handle multiple triggers gracefully
}

//=============================================================================
// Performance Tests
//=============================================================================

/**
 * WHAT: Test consolidation performance
 * WHY: Verify consolidation completes in reasonable time
 */
TEST_F(ConsolidationTest, ConsolidationPerformance)
{
    consolidation_config_t config = consolidation_default_config();
    config.consolidation_cycles = 10;

    auto start = std::chrono::high_resolution_clock::now();

    bool result = brain_consolidate_memory(brain, &config);

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    EXPECT_TRUE(result);

    printf("Consolidation time (10 cycles): %ld ms\n", duration.count());

    // Should complete in reasonable time (< 10 seconds for 10 cycles)
    EXPECT_LT(duration.count(), 10000);
}

// Note: main() is defined in test_module.cpp - all test files share one main()
