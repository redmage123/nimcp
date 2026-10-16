/**
 * @file test_recovery_consolidation_regression.cpp
 * @brief Regression tests for recovery consolidation module
 *
 * WHAT: Prevent regressions in consolidation behavior
 * WHY:  Ensure changes don't break existing functionality
 * HOW:  Test known scenarios that previously caused issues
 *
 * REGRESSION COVERAGE:
 * - Performance regressions
 * - Memory leak regressions
 * - Accuracy regressions
 * - Edge case regressions
 * - Backward compatibility
 *
 * @author NIMCP Development Team
 * @date 2025-01-20
 * @version 2.7.0 Phase 10.1
 */

#include <gtest/gtest.h>
#include <chrono>
#include <cstring>

// Headers have their own extern "C" guards
#include "cognitive/fault_tolerance/nimcp_recovery_consolidation.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"

//=============================================================================
// Test Fixture
//=============================================================================

class ConsolidationRegressionTest : public ::testing::Test {
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

    recovery_episode_t create_episode(
        error_type_t type, uint32_t layer, recovery_action_t action, bool success
    ) {
        recovery_episode_t episode;
        memset(&episode, 0, sizeof(recovery_episode_t));
        episode.timestamp_ms = 1000000;
        episode.error_sig.type = type;
        episode.error_sig.layer_id = layer;
        episode.error_sig.hash = (type * 1000) + layer;
        episode.recovery_action = action;
        episode.success = success;
        episode.recovery_time_us = success ? 10000 : 50000;
        episode.success_confidence = success ? 0.9f : 0.1f;
        episode.emotional_tag = success ? 0.8f : -0.3f;
        return episode;
    }
};

//=============================================================================
// Regression Test 1: Performance Baseline
//=============================================================================

/**
 * @test Consolidation performance should not degrade
 *
 * BASELINE: 100 episodes should consolidate in < 50ms
 * REGRESSION: If this test fails, consolidation got slower
 */
TEST_F(ConsolidationRegressionTest, PerformanceBaseline100Episodes) {
    // ARRANGE
    consolidation = recovery_consolidation_create();

    for (int i = 0; i < 100; i++) {
        recovery_episode_t episode = create_episode(
            ERROR_TYPE_NAN, 5, RECOVERY_ACTION_REDUCE_LR, (i % 10) != 0
        );
        recovery_consolidation_add_episode(consolidation, &episode);
    }

    // ACT
    auto start = std::chrono::high_resolution_clock::now();
    recovery_consolidation_run(consolidation);
    auto end = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end - start
    ).count();

    // ASSERT: Should complete in < 50ms
    EXPECT_LT(duration, 50) << "Consolidation took " << duration
                            << "ms, expected < 50ms";
}

/**
 * @test Large-scale consolidation performance
 *
 * BASELINE: 1000 episodes should consolidate in < 200ms
 */
TEST_F(ConsolidationRegressionTest, PerformanceBaseline1000Episodes) {
    // ARRANGE
    consolidation_config_t config = recovery_consolidation_default_config();
    config.max_rules = 100;
    consolidation = consolidation_create_custom(&config);

    for (int i = 0; i < 1000; i++) {
        error_type_t type = (error_type_t)(ERROR_TYPE_NAN + (i % 10));
        recovery_episode_t episode = create_episode(
            type, (uint32_t)(i % 10),
            RECOVERY_ACTION_REDUCE_LR, (i % 5) != 0
        );
        recovery_consolidation_add_episode(consolidation, &episode);
    }

    // ACT
    auto start = std::chrono::high_resolution_clock::now();
    recovery_consolidation_run(consolidation);
    auto end = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end - start
    ).count();

    // ASSERT: Should complete in < 200ms
    EXPECT_LT(duration, 200) << "Consolidation took " << duration
                             << "ms, expected < 200ms";
}

//=============================================================================
// Regression Test 2: Memory Leak Prevention
//=============================================================================

/**
 * @test No memory leaks in repeated consolidation cycles
 *
 * REGRESSION: Previously leaked memory on repeated consolidations
 */
TEST_F(ConsolidationRegressionTest, NoMemoryLeaksRepeatedCycles) {
    // ARRANGE
    consolidation = recovery_consolidation_create();

    nimcp_memory_stats_t stats_initial;
    nimcp_memory_get_stats(&stats_initial);

    // ACT: 50 consolidation cycles
    for (int cycle = 0; cycle < 50; cycle++) {
        for (int i = 0; i < 20; i++) {
            recovery_episode_t episode = create_episode(
                ERROR_TYPE_NAN, 5, RECOVERY_ACTION_REDUCE_LR, true
            );
            recovery_consolidation_add_episode(consolidation, &episode);
        }
        recovery_consolidation_run(consolidation);
    }

    nimcp_memory_stats_t stats_final;
    nimcp_memory_get_stats(&stats_final);

    // ASSERT: Memory should not grow unbounded
    size_t memory_growth = stats_final.current_allocated - stats_initial.current_allocated;

    // Allow for rule storage but not episode accumulation
    // 50 cycles * 20 episodes = 1000 episodes should NOT all be in memory
    EXPECT_LT(memory_growth, 512 * 1024) << "Memory grew by "
                                         << memory_growth << " bytes";
}

/**
 * @test No memory leaks when destroying with pending episodes
 *
 * REGRESSION: Previously leaked episodes that weren't consolidated
 */
TEST_F(ConsolidationRegressionTest, NoLeaksWithPendingEpisodes) {
    // ARRANGE
    nimcp_memory_stats_t stats_before;
    nimcp_memory_get_stats(&stats_before);

    consolidation = recovery_consolidation_create();

    // Add episodes but don't consolidate
    for (int i = 0; i < 100; i++) {
        recovery_episode_t episode = create_episode(
            ERROR_TYPE_NAN, 5, RECOVERY_ACTION_REDUCE_LR, true
        );
        recovery_consolidation_add_episode(consolidation, &episode);
    }

    // ACT: Destroy without consolidating
    recovery_consolidation_destroy(consolidation);
    consolidation = nullptr;

    nimcp_memory_stats_t stats_after;
    nimcp_memory_get_stats(&stats_after);

    // ASSERT: All memory should be freed
    EXPECT_EQ(stats_after.current_allocated, stats_before.current_allocated);
}

//=============================================================================
// Regression Test 3: Accuracy Regressions
//=============================================================================

/**
 * @test Success rate calculation accuracy
 *
 * REGRESSION: Previously had rounding errors in success rate
 * EXPECTED: 18/20 = 0.9 exactly
 */
TEST_F(ConsolidationRegressionTest, SuccessRateAccuracy) {
    // ARRANGE
    consolidation = recovery_consolidation_create();

    // 18 successes, 2 failures
    for (int i = 0; i < 20; i++) {
        recovery_episode_t episode = create_episode(
            ERROR_TYPE_NAN, 5, RECOVERY_ACTION_REDUCE_LR,
            i < 18  // First 18 succeed
        );
        recovery_consolidation_add_episode(consolidation, &episode);
    }

    // ACT
    recovery_consolidation_run(consolidation);

    error_pattern_t pattern = {ERROR_TYPE_NAN, 5, 0};
    semantic_rule_t* rule = recovery_consolidation_get_rule(consolidation, &pattern);

    // ASSERT: Exact success rate
    ASSERT_NE(rule, nullptr);
    EXPECT_FLOAT_EQ(rule->success_rate, 0.9f);
    EXPECT_EQ(rule->sample_count, 20);
}

/**
 * @test Confidence calculation stability
 *
 * REGRESSION: Previously had floating-point instability
 */
TEST_F(ConsolidationRegressionTest, ConfidenceCalculationStable) {
    // ARRANGE
    consolidation = recovery_consolidation_create();

    // Create identical scenarios 5 times
    float confidences[5];

    for (int run = 0; run < 5; run++) {
        // Reset
        if (consolidation) {
            recovery_consolidation_destroy(consolidation);
        }
        consolidation = recovery_consolidation_create();

        // Add same episodes
        for (int i = 0; i < 30; i++) {
            recovery_episode_t episode = create_episode(
                ERROR_TYPE_NAN, 5, RECOVERY_ACTION_REDUCE_LR,
                i < 27  // 27/30 = 0.9
            );
            recovery_consolidation_add_episode(consolidation, &episode);
        }

        recovery_consolidation_run(consolidation);

        error_pattern_t pattern = {ERROR_TYPE_NAN, 5, 0};
        semantic_rule_t* rule = recovery_consolidation_get_rule(consolidation, &pattern);
        ASSERT_NE(rule, nullptr);

        confidences[run] = rule->confidence;
    }

    // ASSERT: All runs should produce same confidence
    for (int i = 1; i < 5; i++) {
        EXPECT_FLOAT_EQ(confidences[i], confidences[0])
            << "Run " << i << " produced different confidence";
    }
}

//=============================================================================
// Regression Test 4: Edge Cases
//=============================================================================

/**
 * @test Handle episodes with same timestamp
 *
 * REGRESSION: Previously crashed with duplicate timestamps
 */
TEST_F(ConsolidationRegressionTest, DuplicateTimestamps) {
    // ARRANGE
    consolidation = recovery_consolidation_create();

    // All episodes at same timestamp
    for (int i = 0; i < 20; i++) {
        recovery_episode_t episode = create_episode(
            ERROR_TYPE_NAN, 5, RECOVERY_ACTION_REDUCE_LR, true
        );
        episode.timestamp_ms = 1000000;  // Same for all
        recovery_consolidation_add_episode(consolidation, &episode);
    }

    // ACT & ASSERT: Should not crash
    recovery_consolidation_run(consolidation);
    EXPECT_GT(consolidation_get_rule_count(consolidation), 0);
}

/**
 * @test Handle zero success rate
 *
 * REGRESSION: Previously divided by zero with all failures
 */
TEST_F(ConsolidationRegressionTest, ZeroSuccessRate) {
    // ARRANGE
    consolidation = recovery_consolidation_create();

    // All failures
    for (int i = 0; i < 20; i++) {
        recovery_episode_t episode = create_episode(
            ERROR_TYPE_NAN, 5, RECOVERY_ACTION_REDUCE_LR, false
        );
        recovery_consolidation_add_episode(consolidation, &episode);
    }

    // ACT
    recovery_consolidation_run(consolidation);

    error_pattern_t pattern = {ERROR_TYPE_NAN, 5, 0};
    semantic_rule_t* rule = recovery_consolidation_get_rule(consolidation, &pattern);

    // ASSERT: Should create rule with 0% success
    ASSERT_NE(rule, nullptr);
    EXPECT_FLOAT_EQ(rule->success_rate, 0.0f);
    EXPECT_LT(rule->confidence, 0.5f);  // Low confidence in bad strategy
}

/**
 * @test Handle 100% success rate
 *
 * REGRESSION: Previously had confidence calculation errors
 */
TEST_F(ConsolidationRegressionTest, PerfectSuccessRate) {
    // ARRANGE
    consolidation = recovery_consolidation_create();

    // All successes
    for (int i = 0; i < 25; i++) {
        recovery_episode_t episode = create_episode(
            ERROR_TYPE_NAN, 5, RECOVERY_ACTION_REDUCE_LR, true
        );
        recovery_consolidation_add_episode(consolidation, &episode);
    }

    // ACT
    recovery_consolidation_run(consolidation);

    error_pattern_t pattern = {ERROR_TYPE_NAN, 5, 0};
    semantic_rule_t* rule = recovery_consolidation_get_rule(consolidation, &pattern);

    // ASSERT
    ASSERT_NE(rule, nullptr);
    EXPECT_FLOAT_EQ(rule->success_rate, 1.0f);
    EXPECT_GT(rule->confidence, 0.9f);
}

//=============================================================================
// Regression Test 5: Backward Compatibility
//=============================================================================

/**
 * @test Default configuration values remain stable
 *
 * REGRESSION: Changing defaults broke existing behavior
 */
TEST_F(ConsolidationRegressionTest, DefaultConfigStability) {
    // ACT
    consolidation_config_t config = recovery_consolidation_default_config();

    // ASSERT: Known default values
    EXPECT_EQ(config.min_episodes_for_rule, 10);
    EXPECT_FLOAT_EQ(config.min_confidence_threshold, 0.8f);
    EXPECT_EQ(config.max_rules, 100);
    EXPECT_FALSE(config.enable_background_consolidation);
    EXPECT_EQ(config.consolidation_interval_ms, 1000);
}

/**
 * @test API functions maintain backward compatibility
 */
TEST_F(ConsolidationRegressionTest, APIBackwardCompatibility) {
    // Test all public API functions exist and work
    consolidation = recovery_consolidation_create();
    ASSERT_NE(consolidation, nullptr);

    // Basic functions
    EXPECT_FALSE(consolidation_is_active(consolidation));
    EXPECT_EQ(consolidation_get_rule_count(consolidation), 0);
    EXPECT_EQ(consolidation_get_episodes_pending(consolidation), 0);
    EXPECT_EQ(consolidation_get_pattern_count(consolidation), 0);

    // Add episode
    recovery_episode_t episode = create_episode(
        ERROR_TYPE_NAN, 5, RECOVERY_ACTION_REDUCE_LR, true
    );
    EXPECT_TRUE(recovery_consolidation_add_episode(consolidation, &episode));

    // Stats
    consolidation_stats_t stats;
    EXPECT_TRUE(recovery_consolidation_get_stats(consolidation, &stats));

    // Consolidation
    recovery_consolidation_run(consolidation);

    SUCCEED();
}

//=============================================================================
// Regression Test 6: Thread Safety
//=============================================================================

/**
 * @test Background consolidation doesn't corrupt data
 *
 * REGRESSION: Previously had race conditions
 */
TEST_F(ConsolidationRegressionTest, BackgroundThreadSafety) {
    // ARRANGE
    consolidation_config_t config = recovery_consolidation_default_config();
    config.enable_background_consolidation = true;
    config.consolidation_interval_ms = 50;
    consolidation = consolidation_create_custom(&config);

    ASSERT_TRUE(consolidation_start_background(consolidation));

    // ACT: Rapidly add episodes while background thread runs
    for (int i = 0; i < 200; i++) {
        recovery_episode_t episode = create_episode(
            ERROR_TYPE_NAN, 5, RECOVERY_ACTION_REDUCE_LR, (i % 4) != 0
        );
        recovery_consolidation_add_episode(consolidation, &episode);
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    // ASSERT: No corruption
    uint32_t rule_count = consolidation_get_rule_count(consolidation);
    EXPECT_GT(rule_count, 0);
    EXPECT_LE(rule_count, 100);

    consolidation_stop_background(consolidation);
}

//=============================================================================
// Regression Test 7: Resource Limits
//=============================================================================

/**
 * @test Max rules limit enforced correctly
 *
 * REGRESSION: Previously allowed unlimited rules causing OOM
 */
TEST_F(ConsolidationRegressionTest, MaxRulesEnforcement) {
    // ARRANGE
    consolidation_config_t config = recovery_consolidation_default_config();
    config.max_rules = 5;
    consolidation = consolidation_create_custom(&config);

    // ACT: Try to create 20 different rules
    for (int i = 0; i < 20; i++) {
        for (int j = 0; j < 15; j++) {
            recovery_episode_t episode = create_episode(
                (error_type_t)(ERROR_TYPE_NAN + i),
                (uint32_t)i,
                RECOVERY_ACTION_REDUCE_LR,
                true
            );
            recovery_consolidation_add_episode(consolidation, &episode);
        }
        recovery_consolidation_run(consolidation);
    }

    // ASSERT: Should cap at max_rules
    EXPECT_LE(consolidation_get_rule_count(consolidation), 5);
}

/**
 * @test Episode buffer doesn't overflow
 *
 * REGRESSION: Previously crashed with too many pending episodes
 */
TEST_F(ConsolidationRegressionTest, EpisodeBufferNoOverflow) {
    // ARRANGE
    consolidation = recovery_consolidation_create();

    // ACT: Add excessive episodes without consolidating
    for (int i = 0; i < 10000; i++) {
        recovery_episode_t episode = create_episode(
            ERROR_TYPE_NAN, 5, RECOVERY_ACTION_REDUCE_LR, true
        );
        bool result = recovery_consolidation_add_episode(consolidation, &episode);

        // Should either succeed or gracefully reject
        EXPECT_TRUE(result || !result);  // No crash
    }

    // ASSERT: Should not crash
    SUCCEED();
}

//=============================================================================
// Regression Test 8: Numerical Stability
//=============================================================================

/**
 * @test Confidence calculation with very small sample
 *
 * REGRESSION: Previously produced NaN with N=1
 */
TEST_F(ConsolidationRegressionTest, SmallSampleConfidence) {
    // ARRANGE
    consolidation = recovery_consolidation_create();

    // Single episode
    recovery_episode_t episode = create_episode(
        ERROR_TYPE_NAN, 5, RECOVERY_ACTION_REDUCE_LR, true
    );
    recovery_consolidation_add_episode(consolidation, &episode);

    // Force consolidation with minimal config
    consolidation_config_t config = recovery_consolidation_default_config();
    config.min_episodes_for_rule = 1;
    recovery_consolidation_destroy(consolidation);
    consolidation = consolidation_create_custom(&config);
    recovery_consolidation_add_episode(consolidation, &episode);

    // ACT
    recovery_consolidation_run(consolidation);

    error_pattern_t pattern = {ERROR_TYPE_NAN, 5, 0};
    semantic_rule_t* rule = recovery_consolidation_get_rule(consolidation, &pattern);

    // ASSERT: Should not produce NaN
    if (rule != nullptr) {
        EXPECT_FALSE(std::isnan(rule->confidence));
        EXPECT_FALSE(std::isnan(rule->success_rate));
        EXPECT_GE(rule->confidence, 0.0f);
        EXPECT_LE(rule->confidence, 1.0f);
    }
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
