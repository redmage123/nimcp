/**
 * @file test_recovery_consolidation_integration.cpp
 * @brief Integration tests for recovery consolidation with other modules
 *
 * WHAT: Test consolidation integration with episodic memory and recovery system
 * WHY:  Verify end-to-end workflows and cross-module interactions
 * HOW:  Simulate realistic recovery scenarios with episode storage
 *
 * INTEGRATION SCENARIOS:
 * - Episodic memory → Consolidation → Semantic rules
 * - Multiple recovery cycles → Pattern learning
 * - Rule application in future recoveries
 * - Memory transfer and persistence
 *
 * @author NIMCP Development Team
 * @date 2025-01-20
 * @version 2.7.0 Phase 10.1
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <cstring>

// Headers have their own extern "C" guards
#include "cognitive/fault_tolerance/nimcp_recovery_consolidation.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"

//=============================================================================
// Test Fixture
//=============================================================================

class ConsolidationIntegrationTest : public ::testing::Test {
protected:
    recovery_consolidation_t* consolidation;

    void SetUp() override {
        nimcp_memory_init();
        nimcp_memory_enable_tracking(true);
        log_init(nullptr);

        consolidation = nullptr;
    }

    void TearDown() override {
        if (consolidation) {
            recovery_consolidation_destroy(consolidation);
            consolidation = nullptr;
        }

        nimcp_memory_check_leaks();
        log_close();
    }

    // Helper: Simulate recovery episode
    recovery_episode_t simulate_recovery(
        error_type_t error_type,
        uint32_t layer_id,
        recovery_action_t action,
        bool* success_out
    ) {
        recovery_episode_t episode;
        memset(&episode, 0, sizeof(recovery_episode_t));

        // Simulate error detection
        episode.timestamp_ms = get_current_time_ms();
        episode.error_sig.type = error_type;
        episode.error_sig.layer_id = layer_id;
        episode.error_sig.hash = (error_type * 1000) + layer_id;

        // Simulate recovery attempt
        episode.recovery_action = action;

        // Simulate outcome (probabilistic)
        bool success = (rand() % 100) < 80;  // 80% success rate
        episode.success = success;
        episode.recovery_time_us = success ? 15000 : 50000;
        episode.success_confidence = success ? 0.85f : 0.2f;
        episode.emotional_tag = success ? 0.7f : -0.5f;

        if (success_out) *success_out = success;
        return episode;
    }

    // Get current time in milliseconds
    uint64_t get_current_time_ms() {
        auto now = std::chrono::system_clock::now();
        auto duration = now.time_since_epoch();
        return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
    }
};

//=============================================================================
// Integration Test 1: Episodic to Semantic Transfer
//=============================================================================

/**
 * @test End-to-end: Episodes → Consolidation → Semantic Rules
 *
 * WHAT: Simulate multiple recovery episodes and verify rule creation
 * WHY:  Test complete pipeline from experience to learned rules
 * HOW:  30 NaN recoveries → consolidate → verify rule extracted
 */
TEST_F(ConsolidationIntegrationTest, EpisodicToSemanticTransfer) {
    // ARRANGE
    consolidation = recovery_consolidation_create();
    ASSERT_NE(consolidation, nullptr);

    // SIMULATE: 30 recovery episodes for NaN errors
    int success_count = 0;
    for (int i = 0; i < 30; i++) {
        bool success;
        recovery_episode_t episode = simulate_recovery(
            ERROR_TYPE_NAN, 5, RECOVERY_ACTION_REDUCE_LR, &success
        );

        if (success) success_count++;
        recovery_consolidation_add_episode(consolidation, &episode);
    }

    // ACT: Run consolidation
    recovery_consolidation_run(consolidation);

    // ASSERT: Rule should be created
    EXPECT_GT(consolidation_get_rule_count(consolidation), 0);

    // Verify rule quality
    error_pattern_t pattern;
    pattern.type = ERROR_TYPE_NAN;
    pattern.layer_id = 5;

    semantic_rule_t* rule = recovery_consolidation_get_rule(consolidation, &pattern);
    ASSERT_NE(rule, nullptr);

    // Success rate should be ~80% (probabilistic)
    EXPECT_GT(rule->success_rate, 0.6f);
    EXPECT_LT(rule->success_rate, 1.0f);
    EXPECT_EQ(rule->sample_count, 30);
    EXPECT_GT(rule->confidence, 0.8f);  // High confidence with N=30
}

//=============================================================================
// Integration Test 2: Multi-Pattern Learning
//=============================================================================

/**
 * @test Learn multiple patterns from different error types
 */
TEST_F(ConsolidationIntegrationTest, MultiPatternLearning) {
    // ARRANGE
    consolidation = recovery_consolidation_create();

    // SIMULATE: Different error types with different recovery actions
    // Pattern 1: NaN → Reduce LR (20 episodes)
    for (int i = 0; i < 20; i++) {
        recovery_episode_t episode = simulate_recovery(
            ERROR_TYPE_NAN, 5, RECOVERY_ACTION_REDUCE_LR, nullptr
        );
        recovery_consolidation_add_episode(consolidation, &episode);
    }

    // Pattern 2: Overflow → Reload checkpoint (20 episodes)
    for (int i = 0; i < 20; i++) {
        recovery_episode_t episode = simulate_recovery(
            ERROR_TYPE_OVERFLOW, 3, RECOVERY_ACTION_RELOAD_CHECKPOINT, nullptr
        );
        recovery_consolidation_add_episode(consolidation, &episode);
    }

    // Pattern 3: Timeout → Increase timeout (20 episodes)
    for (int i = 0; i < 20; i++) {
        recovery_episode_t episode = simulate_recovery(
            ERROR_TYPE_TIMEOUT, 7, RECOVERY_ACTION_INCREASE_TIMEOUT, nullptr
        );
        recovery_consolidation_add_episode(consolidation, &episode);
    }

    // ACT
    recovery_consolidation_run(consolidation);

    // ASSERT: Should create 3 distinct rules
    EXPECT_EQ(consolidation_get_rule_count(consolidation), 3);

    // Verify each pattern
    error_pattern_t pattern_nan = {ERROR_TYPE_NAN, 5, 0};
    semantic_rule_t* rule_nan = recovery_consolidation_get_rule(consolidation, &pattern_nan);
    ASSERT_NE(rule_nan, nullptr);
    EXPECT_EQ(rule_nan->action, RECOVERY_ACTION_REDUCE_LR);

    error_pattern_t pattern_overflow = {ERROR_TYPE_OVERFLOW, 3, 0};
    semantic_rule_t* rule_overflow = recovery_consolidation_get_rule(consolidation, &pattern_overflow);
    ASSERT_NE(rule_overflow, nullptr);
    EXPECT_EQ(rule_overflow->action, RECOVERY_ACTION_RELOAD_CHECKPOINT);

    error_pattern_t pattern_timeout = {ERROR_TYPE_TIMEOUT, 7, 0};
    semantic_rule_t* rule_timeout = recovery_consolidation_get_rule(consolidation, &pattern_timeout);
    ASSERT_NE(rule_timeout, nullptr);
    EXPECT_EQ(rule_timeout->action, RECOVERY_ACTION_INCREASE_TIMEOUT);
}

//=============================================================================
// Integration Test 3: Incremental Learning
//=============================================================================

/**
 * @test Incremental rule refinement with additional episodes
 *
 * WHAT: Add more episodes after initial consolidation
 * WHY:  Verify rules can be updated with new evidence
 * HOW:  Consolidate 20 episodes, add 30 more, consolidate again
 */
TEST_F(ConsolidationIntegrationTest, IncrementalLearning) {
    // ARRANGE
    consolidation = recovery_consolidation_create();

    // PHASE 1: Initial learning (20 episodes)
    for (int i = 0; i < 20; i++) {
        recovery_episode_t episode = simulate_recovery(
            ERROR_TYPE_NAN, 5, RECOVERY_ACTION_REDUCE_LR, nullptr
        );
        recovery_consolidation_add_episode(consolidation, &episode);
    }

    recovery_consolidation_run(consolidation);
    uint32_t initial_rule_count = consolidation_get_rule_count(consolidation);

    // Get initial rule
    error_pattern_t pattern = {ERROR_TYPE_NAN, 5, 0};
    semantic_rule_t* initial_rule = recovery_consolidation_get_rule(consolidation, &pattern);
    ASSERT_NE(initial_rule, nullptr);
    float initial_success_rate = initial_rule->success_rate;

    // PHASE 2: Additional episodes (30 more)
    for (int i = 0; i < 30; i++) {
        recovery_episode_t episode = simulate_recovery(
            ERROR_TYPE_NAN, 5, RECOVERY_ACTION_REDUCE_LR, nullptr
        );
        recovery_consolidation_add_episode(consolidation, &episode);
    }

    // ACT
    recovery_consolidation_run(consolidation);

    // ASSERT: Rule should be updated
    semantic_rule_t* updated_rule = recovery_consolidation_get_rule(consolidation, &pattern);
    ASSERT_NE(updated_rule, nullptr);

    // Sample count should increase
    EXPECT_GT(updated_rule->sample_count, initial_rule->sample_count);
    // Confidence should increase (more data)
    EXPECT_GE(updated_rule->confidence, initial_rule->confidence);
}

//=============================================================================
// Integration Test 4: Background Consolidation
//=============================================================================

/**
 * @test Background consolidation thread processes episodes
 *
 * WHAT: Test asynchronous consolidation
 * WHY:  Verify background processing doesn't block main thread
 * HOW:  Start background thread, add episodes, wait, verify rules
 */
TEST_F(ConsolidationIntegrationTest, BackgroundConsolidation) {
    // ARRANGE
    consolidation_config_t config = recovery_consolidation_default_config();
    config.enable_background_consolidation = true;
    config.consolidation_interval_ms = 100;  // Fast for testing
    consolidation = consolidation_create_custom(&config);

    // Start background consolidation
    ASSERT_TRUE(consolidation_start_background(consolidation));

    // ACT: Add episodes while background thread runs
    for (int i = 0; i < 25; i++) {
        recovery_episode_t episode = simulate_recovery(
            ERROR_TYPE_NAN, 5, RECOVERY_ACTION_REDUCE_LR, nullptr
        );
        recovery_consolidation_add_episode(consolidation, &episode);

        // Small delay to simulate realistic timing
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    // Wait for background consolidation
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // ASSERT: Rules should be created by background thread
    EXPECT_GT(consolidation_get_rule_count(consolidation), 0);

    // Cleanup
    consolidation_stop_background(consolidation);
}

//=============================================================================
// Integration Test 5: High-Load Scenario
//=============================================================================

/**
 * @test Handle high volume of episodes
 *
 * WHAT: Stress test with 500 episodes
 * WHY:  Verify scalability and performance
 * HOW:  Rapidly add 500 episodes, consolidate, verify correctness
 */
TEST_F(ConsolidationIntegrationTest, HighLoadScenario) {
    // ARRANGE
    consolidation_config_t config = recovery_consolidation_default_config();
    config.max_rules = 50;  // Allow many rules
    consolidation = consolidation_create_custom(&config);

    // ACT: Add 500 episodes (mix of 5 error types)
    auto start_time = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 500; i++) {
        error_type_t error_type = (error_type_t)(ERROR_TYPE_NAN + (i % 5));
        recovery_action_t action = (recovery_action_t)(RECOVERY_ACTION_REDUCE_LR + (i % 5));

        recovery_episode_t episode = simulate_recovery(
            error_type, (uint32_t)(i % 10), action, nullptr
        );
        recovery_consolidation_add_episode(consolidation, &episode);
    }

    recovery_consolidation_run(consolidation);

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time
    ).count();

    // ASSERT: Should complete in reasonable time (< 1 second)
    EXPECT_LT(duration, 1000);

    // Should create multiple rules
    EXPECT_GT(consolidation_get_rule_count(consolidation), 0);
    EXPECT_LE(consolidation_get_rule_count(consolidation), 50);

    // Get statistics
    consolidation_stats_t stats;
    ASSERT_TRUE(recovery_consolidation_get_stats(consolidation, &stats));
    EXPECT_EQ(stats.total_episodes_processed, 500);
}

//=============================================================================
// Integration Test 6: Rule Application in Recovery
//=============================================================================

/**
 * @test Use learned rules for faster recovery decisions
 *
 * WHAT: Simulate learning phase then apply rules to new errors
 * WHY:  Verify rules actually improve recovery
 * HOW:  Learn from 30 episodes, then use rule for instant decision
 */
TEST_F(ConsolidationIntegrationTest, RuleApplicationInRecovery) {
    // ARRANGE: Learning phase
    consolidation = recovery_consolidation_create();

    for (int i = 0; i < 30; i++) {
        recovery_episode_t episode = simulate_recovery(
            ERROR_TYPE_NAN, 5, RECOVERY_ACTION_REDUCE_LR, nullptr
        );
        recovery_consolidation_add_episode(consolidation, &episode);
    }

    recovery_consolidation_run(consolidation);

    // ACT: New error occurs, lookup rule
    error_pattern_t new_error = {ERROR_TYPE_NAN, 5, 0};
    semantic_rule_t* rule = recovery_consolidation_get_rule(consolidation, &new_error);

    // ASSERT: Rule should provide instant recovery action
    ASSERT_NE(rule, nullptr);
    EXPECT_EQ(rule->action, RECOVERY_ACTION_REDUCE_LR);
    EXPECT_GT(rule->success_rate, 0.5f);

    // Verify confidence is high enough to use
    EXPECT_GT(rule->confidence, 0.7f);
}

//=============================================================================
// Integration Test 7: Memory Efficiency
//=============================================================================

/**
 * @test Verify memory usage stays bounded
 *
 * WHAT: Test memory doesn't leak during consolidation
 * WHY:  Ensure long-running systems don't accumulate memory
 * HOW:  Multiple consolidation cycles, check memory stats
 */
TEST_F(ConsolidationIntegrationTest, MemoryEfficiency) {
    // ARRANGE
    consolidation = recovery_consolidation_create();

    nimcp_memory_stats_t stats_before;
    nimcp_memory_get_stats(&stats_before);

    // ACT: Multiple consolidation cycles
    for (int cycle = 0; cycle < 10; cycle++) {
        // Add episodes
        for (int i = 0; i < 50; i++) {
            recovery_episode_t episode = simulate_recovery(
                ERROR_TYPE_NAN, 5, RECOVERY_ACTION_REDUCE_LR, nullptr
            );
            recovery_consolidation_add_episode(consolidation, &episode);
        }

        // Consolidate
        recovery_consolidation_run(consolidation);
    }

    nimcp_memory_stats_t stats_after;
    nimcp_memory_get_stats(&stats_after);

    // ASSERT: Memory growth should be bounded
    size_t memory_growth = stats_after.current_allocated - stats_before.current_allocated;

    // Growth should be reasonable (< 1MB for 500 episodes)
    EXPECT_LT(memory_growth, 1024 * 1024);
}

//=============================================================================
// Integration Test 8: Concurrent Access
//=============================================================================

/**
 * @test Thread safety with concurrent episode additions
 *
 * WHAT: Multiple threads add episodes simultaneously
 * WHY:  Verify thread safety in realistic scenarios
 * HOW:  4 threads each add 50 episodes, verify no corruption
 */
TEST_F(ConsolidationIntegrationTest, ConcurrentAccess) {
    // ARRANGE
    consolidation_config_t config = recovery_consolidation_default_config();
    config.enable_background_consolidation = false;  // Test manual mode
    consolidation = consolidation_create_custom(&config);

    // ACT: Multiple threads add episodes
    std::vector<std::thread> threads;
    const int num_threads = 4;
    const int episodes_per_thread = 50;

    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([this, t, episodes_per_thread]() {
            for (int i = 0; i < episodes_per_thread; i++) {
                recovery_episode_t episode = simulate_recovery(
                    ERROR_TYPE_NAN, 5, RECOVERY_ACTION_REDUCE_LR, nullptr
                );
                recovery_consolidation_add_episode(consolidation, &episode);
            }
        });
    }

    // Wait for all threads
    for (auto& thread : threads) {
        thread.join();
    }

    // ASSERT: All episodes should be recorded
    EXPECT_EQ(consolidation_get_episodes_pending(consolidation),
              num_threads * episodes_per_thread);

    // Consolidate should work
    recovery_consolidation_run(consolidation);
    EXPECT_GT(consolidation_get_rule_count(consolidation), 0);
}

//=============================================================================
// Integration Test 9: Recovery After Failure
//=============================================================================

/**
 * @test System recovers if consolidation fails mid-process
 */
TEST_F(ConsolidationIntegrationTest, RecoveryAfterFailure) {
    // ARRANGE
    consolidation = recovery_consolidation_create();

    // Add some episodes
    for (int i = 0; i < 20; i++) {
        recovery_episode_t episode = simulate_recovery(
            ERROR_TYPE_NAN, 5, RECOVERY_ACTION_REDUCE_LR, nullptr
        );
        recovery_consolidation_add_episode(consolidation, &episode);
    }

    // ACT: Simulate failure scenario (destroy and recreate)
    recovery_consolidation_destroy(consolidation);
    consolidation = recovery_consolidation_create();

    // Add new episodes
    for (int i = 0; i < 25; i++) {
        recovery_episode_t episode = simulate_recovery(
            ERROR_TYPE_NAN, 5, RECOVERY_ACTION_REDUCE_LR, nullptr
        );
        recovery_consolidation_add_episode(consolidation, &episode);
    }

    recovery_consolidation_run(consolidation);

    // ASSERT: Should work normally
    EXPECT_GT(consolidation_get_rule_count(consolidation), 0);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
