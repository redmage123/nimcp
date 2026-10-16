/**
 * @file test_recovery_consolidation.cpp
 * @brief Unit tests for recovery consolidation module
 *
 * WHAT: Test episodic→semantic memory consolidation
 * WHY:  Verify pattern extraction and rule creation from recovery episodes
 * HOW:  Test each function with AAA pattern (Arrange-Act-Assert)
 *
 * TEST COVERAGE:
 * - Consolidation creation/destruction
 * - Pattern extraction from episodes
 * - Semantic rule creation
 * - Success rate computation
 * - Statistical confidence calculation
 * - Background consolidation
 * - Error conditions
 *
 * @author NIMCP Development Team
 * @date 2025-01-20
 * @version 2.7.0 Phase 10.1
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

// Headers have their own extern "C" guards
#include "cognitive/fault_tolerance/nimcp_recovery_consolidation.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"

//=============================================================================
// Test Fixture
//=============================================================================

class RecoveryConsolidationTest : public ::testing::Test {
protected:
    recovery_consolidation_t* consolidation;

    void SetUp() override {
        // Initialize memory tracking
        nimcp_memory_init();
        nimcp_memory_enable_tracking(true);

        // Initialize logging
        log_init(nullptr);

        consolidation = nullptr;
    }

    void TearDown() override {
        // Cleanup consolidation if created
        if (consolidation) {
            recovery_consolidation_destroy(consolidation);
            consolidation = nullptr;
        }

        // Check for memory leaks
        nimcp_memory_check_leaks();
        log_close();
    }

    // Helper: Create test episode
    recovery_episode_t create_test_episode(
        error_type_t error_type,
        uint32_t layer_id,
        recovery_action_t action,
        bool success,
        float emotional_tag = 0.5f
    ) {
        recovery_episode_t episode;
        memset(&episode, 0, sizeof(recovery_episode_t));

        episode.timestamp_ms = 1000000;
        episode.error_sig.type = error_type;
        episode.error_sig.layer_id = layer_id;
        episode.error_sig.hash = (error_type * 1000) + layer_id;
        episode.recovery_action = action;
        episode.success = success;
        episode.recovery_time_us = success ? 10000 : 50000;
        episode.success_confidence = success ? 0.9f : 0.1f;
        episode.emotional_tag = emotional_tag;

        return episode;
    }

    // Helper: Create similar episodes for pattern
    void create_similar_episodes(
        recovery_episode_t* episodes,
        uint32_t count,
        error_type_t error_type,
        recovery_action_t action,
        float success_rate = 0.9f
    ) {
        for (uint32_t i = 0; i < count; i++) {
            bool success = (i < (uint32_t)(count * success_rate));
            episodes[i] = create_test_episode(
                error_type, 5, action, success,
                success ? 0.8f : -0.3f
            );
            // Vary timestamps
            episodes[i].timestamp_ms = 1000000 + (i * 10000);
        }
    }
};

//=============================================================================
// Test 1: Consolidation Creation and Destruction
//=============================================================================

/**
 * @test Consolidation creation with default config
 *
 * WHAT: Verify consolidation structure is created properly
 * WHY:  Ensure initialization sets correct defaults
 * HOW:  Create, verify fields, destroy
 */
TEST_F(RecoveryConsolidationTest, CreateWithDefaultConfig) {
    // ARRANGE & ACT
    consolidation = recovery_consolidation_create();

    // ASSERT
    ASSERT_NE(consolidation, nullptr);
    EXPECT_FALSE(consolidation_is_active(consolidation));
    EXPECT_EQ(consolidation_get_rule_count(consolidation), 0);
    EXPECT_EQ(consolidation_get_episodes_pending(consolidation), 0);
}

/**
 * @test Consolidation creation with custom config
 */
TEST_F(RecoveryConsolidationTest, CreateWithCustomConfig) {
    // ARRANGE
    consolidation_config_t config = recovery_consolidation_default_config();
    config.min_episodes_for_rule = 15;
    config.min_confidence_threshold = 0.90f;
    config.enable_background_consolidation = true;

    // ACT
    consolidation = consolidation_create_custom(&config);

    // ASSERT
    ASSERT_NE(consolidation, nullptr);
    EXPECT_FALSE(consolidation_is_active(consolidation));
}

/**
 * @test NULL parameter handling in creation
 */
TEST_F(RecoveryConsolidationTest, CreateWithNullConfig) {
    // ACT
    consolidation = consolidation_create_custom(nullptr);

    // ASSERT - Should use defaults
    ASSERT_NE(consolidation, nullptr);
}

/**
 * @test Consolidation destruction
 */
TEST_F(RecoveryConsolidationTest, Destroy) {
    // ARRANGE
    consolidation = recovery_consolidation_create();
    ASSERT_NE(consolidation, nullptr);

    // ACT
    recovery_consolidation_destroy(consolidation);
    consolidation = nullptr;  // Prevent double-free in TearDown

    // ASSERT - No crash, no leaks (checked by TearDown)
    SUCCEED();
}

/**
 * @test Destroy with NULL pointer (should be safe)
 */
TEST_F(RecoveryConsolidationTest, DestroyNull) {
    // ACT & ASSERT - Should not crash
    recovery_consolidation_destroy(nullptr);
    SUCCEED();
}

//=============================================================================
// Test 2: Episode Addition and Management
//=============================================================================

/**
 * @test Add episode to consolidation queue
 */
TEST_F(RecoveryConsolidationTest, AddEpisode) {
    // ARRANGE
    consolidation = recovery_consolidation_create();
    recovery_episode_t episode = create_test_episode(
        ERROR_TYPE_NAN, 5, RECOVERY_ACTION_REDUCE_LR, true
    );

    // ACT
    bool result = recovery_consolidation_add_episode(consolidation, &episode);

    // ASSERT
    EXPECT_TRUE(result);
    EXPECT_EQ(consolidation_get_episodes_pending(consolidation), 1);
}

/**
 * @test Add multiple episodes
 */
TEST_F(RecoveryConsolidationTest, AddMultipleEpisodes) {
    // ARRANGE
    consolidation = recovery_consolidation_create();
    recovery_episode_t episodes[5];
    create_similar_episodes(episodes, 5, ERROR_TYPE_NAN,
                          RECOVERY_ACTION_REDUCE_LR, 0.8f);

    // ACT
    for (int i = 0; i < 5; i++) {
        recovery_consolidation_add_episode(consolidation, &episodes[i]);
    }

    // ASSERT
    EXPECT_EQ(consolidation_get_episodes_pending(consolidation), 5);
}

/**
 * @test Add episode with NULL consolidation
 */
TEST_F(RecoveryConsolidationTest, AddEpisodeNullConsolidation) {
    // ARRANGE
    recovery_episode_t episode = create_test_episode(
        ERROR_TYPE_NAN, 5, RECOVERY_ACTION_REDUCE_LR, true
    );

    // ACT
    bool result = recovery_consolidation_add_episode(nullptr, &episode);

    // ASSERT
    EXPECT_FALSE(result);
}

/**
 * @test Add NULL episode
 */
TEST_F(RecoveryConsolidationTest, AddNullEpisode) {
    // ARRANGE
    consolidation = recovery_consolidation_create();

    // ACT
    bool result = recovery_consolidation_add_episode(consolidation, nullptr);

    // ASSERT
    EXPECT_FALSE(result);
    EXPECT_EQ(consolidation_get_episodes_pending(consolidation), 0);
}

//=============================================================================
// Test 3: Pattern Extraction
//=============================================================================

/**
 * @test Extract patterns from similar episodes
 *
 * WHAT: Test pattern extraction logic
 * WHY:  Core functionality for consolidation
 * HOW:  Create 20 similar NaN episodes, extract pattern
 */
TEST_F(RecoveryConsolidationTest, ExtractPatternsFromEpisodes) {
    // ARRANGE
    consolidation = recovery_consolidation_create();
    recovery_episode_t episodes[20];
    create_similar_episodes(episodes, 20, ERROR_TYPE_NAN,
                          RECOVERY_ACTION_REDUCE_LR, 0.9f);  // 90% success

    // ACT
    consolidation_extract_patterns(consolidation, episodes, 20);

    // ASSERT
    // Should identify NaN → Reduce LR pattern
    EXPECT_GT(consolidation_get_pattern_count(consolidation), 0);
}

/**
 * @test Pattern extraction with insufficient episodes
 */
TEST_F(RecoveryConsolidationTest, ExtractPatternsInsufficientData) {
    // ARRANGE
    consolidation = recovery_consolidation_create();
    recovery_episode_t episodes[5];
    create_similar_episodes(episodes, 5, ERROR_TYPE_NAN,
                          RECOVERY_ACTION_REDUCE_LR, 1.0f);

    // ACT
    consolidation_extract_patterns(consolidation, episodes, 5);

    // ASSERT
    // Should not create pattern with < min_episodes (default 10)
    EXPECT_EQ(consolidation_get_pattern_count(consolidation), 0);
}

/**
 * @test Pattern extraction with NULL parameters
 */
TEST_F(RecoveryConsolidationTest, ExtractPatternsNullParams) {
    // ARRANGE
    consolidation = recovery_consolidation_create();
    recovery_episode_t episodes[10];
    create_similar_episodes(episodes, 10, ERROR_TYPE_NAN,
                          RECOVERY_ACTION_REDUCE_LR, 1.0f);

    // ACT & ASSERT - Should not crash
    consolidation_extract_patterns(nullptr, episodes, 10);
    consolidation_extract_patterns(consolidation, nullptr, 10);
    consolidation_extract_patterns(consolidation, episodes, 0);

    SUCCEED();
}

//=============================================================================
// Test 4: Semantic Rule Creation
//=============================================================================

/**
 * @test Create semantic rule from similar episodes
 *
 * WHAT: Test rule creation with success rate computation
 * WHY:  Verify consolidation creates valid rules
 * HOW:  20 episodes (18 success) → rule with 90% success rate
 */
TEST_F(RecoveryConsolidationTest, CreateSemanticRule) {
    // ARRANGE
    consolidation = recovery_consolidation_create();
    recovery_episode_t episodes[20];
    create_similar_episodes(episodes, 20, ERROR_TYPE_NAN,
                          RECOVERY_ACTION_REDUCE_LR, 0.9f);  // 18/20 success

    // Create pointers array
    const recovery_episode_t* episode_ptrs[20];
    for (int i = 0; i < 20; i++) {
        episode_ptrs[i] = &episodes[i];
    }

    // ACT
    semantic_rule_t rule = consolidation_create_rule(
        consolidation, episode_ptrs, 20
    );

    // ASSERT
    EXPECT_EQ(rule.pattern.type, ERROR_TYPE_NAN);
    EXPECT_EQ(rule.action, RECOVERY_ACTION_REDUCE_LR);
    EXPECT_NEAR(rule.success_rate, 0.9f, 0.01f);  // 18/20 = 0.9
    EXPECT_EQ(rule.sample_count, 20);
    EXPECT_GT(rule.confidence, 0.0f);
}

/**
 * @test Rule creation with perfect success rate
 */
TEST_F(RecoveryConsolidationTest, CreateRulePerfectSuccess) {
    // ARRANGE
    consolidation = recovery_consolidation_create();
    recovery_episode_t episodes[15];
    create_similar_episodes(episodes, 15, ERROR_TYPE_OVERFLOW,
                          RECOVERY_ACTION_RELOAD_CHECKPOINT, 1.0f);

    const recovery_episode_t* episode_ptrs[15];
    for (int i = 0; i < 15; i++) {
        episode_ptrs[i] = &episodes[i];
    }

    // ACT
    semantic_rule_t rule = consolidation_create_rule(
        consolidation, episode_ptrs, 15
    );

    // ASSERT
    EXPECT_FLOAT_EQ(rule.success_rate, 1.0f);
    EXPECT_EQ(rule.sample_count, 15);
}

/**
 * @test Rule creation with low success rate
 */
TEST_F(RecoveryConsolidationTest, CreateRuleLowSuccess) {
    // ARRANGE
    consolidation = recovery_consolidation_create();
    recovery_episode_t episodes[20];
    create_similar_episodes(episodes, 20, ERROR_TYPE_TIMEOUT,
                          RECOVERY_ACTION_INCREASE_TIMEOUT, 0.4f);  // Only 40%

    const recovery_episode_t* episode_ptrs[20];
    for (int i = 0; i < 20; i++) {
        episode_ptrs[i] = &episodes[i];
    }

    // ACT
    semantic_rule_t rule = consolidation_create_rule(
        consolidation, episode_ptrs, 20
    );

    // ASSERT
    EXPECT_NEAR(rule.success_rate, 0.4f, 0.01f);
    EXPECT_LT(rule.confidence, 0.95f);  // Low success = lower confidence
}

//=============================================================================
// Test 5: Statistical Confidence Calculation
//=============================================================================

/**
 * @test Confidence calculation with large sample
 *
 * WHAT: Verify statistical confidence increases with sample size
 * WHY:  Larger samples → higher confidence
 * HOW:  Compare confidence for N=10 vs N=50
 */
TEST_F(RecoveryConsolidationTest, ConfidenceIncreasesWithSampleSize) {
    // ARRANGE
    consolidation = recovery_consolidation_create();

    // Small sample
    recovery_episode_t episodes_small[10];
    create_similar_episodes(episodes_small, 10, ERROR_TYPE_NAN,
                          RECOVERY_ACTION_REDUCE_LR, 0.8f);
    const recovery_episode_t* ptrs_small[10];
    for (int i = 0; i < 10; i++) ptrs_small[i] = &episodes_small[i];

    // Large sample
    recovery_episode_t episodes_large[50];
    create_similar_episodes(episodes_large, 50, ERROR_TYPE_NAN,
                          RECOVERY_ACTION_REDUCE_LR, 0.8f);
    const recovery_episode_t* ptrs_large[50];
    for (int i = 0; i < 50; i++) ptrs_large[i] = &episodes_large[i];

    // ACT
    semantic_rule_t rule_small = consolidation_create_rule(
        consolidation, ptrs_small, 10
    );
    semantic_rule_t rule_large = consolidation_create_rule(
        consolidation, ptrs_large, 50
    );

    // ASSERT
    EXPECT_GT(rule_large.confidence, rule_small.confidence);
}

/**
 * @test Confidence calculation formula
 */
TEST_F(RecoveryConsolidationTest, ConfidenceCalculation) {
    // ARRANGE
    consolidation = recovery_consolidation_create();
    recovery_episode_t episodes[30];
    create_similar_episodes(episodes, 30, ERROR_TYPE_NAN,
                          RECOVERY_ACTION_REDUCE_LR, 0.9f);

    const recovery_episode_t* ptrs[30];
    for (int i = 0; i < 30; i++) ptrs[i] = &episodes[i];

    // ACT
    semantic_rule_t rule = consolidation_create_rule(consolidation, ptrs, 30);

    // ASSERT
    // With N=30, p=0.9, confidence should be high (>0.9)
    EXPECT_GT(rule.confidence, 0.85f);
    EXPECT_LE(rule.confidence, 1.0f);
}

//=============================================================================
// Test 6: Semantic Memory Management
//=============================================================================

/**
 * @test Add semantic rule to memory
 */
TEST_F(RecoveryConsolidationTest, AddSemanticRule) {
    // ARRANGE
    consolidation = recovery_consolidation_create();
    recovery_episode_t episodes[20];
    create_similar_episodes(episodes, 20, ERROR_TYPE_NAN,
                          RECOVERY_ACTION_REDUCE_LR, 0.9f);

    const recovery_episode_t* ptrs[20];
    for (int i = 0; i < 20; i++) ptrs[i] = &episodes[i];

    semantic_rule_t rule = consolidation_create_rule(consolidation, ptrs, 20);

    // ACT
    bool result = consolidation_add_rule(consolidation, &rule);

    // ASSERT
    EXPECT_TRUE(result);
    EXPECT_EQ(consolidation_get_rule_count(consolidation), 1);
}

/**
 * @test Retrieve semantic rule by pattern
 */
TEST_F(RecoveryConsolidationTest, GetRuleByPattern) {
    // ARRANGE
    consolidation = recovery_consolidation_create();
    recovery_episode_t episodes[20];
    create_similar_episodes(episodes, 20, ERROR_TYPE_NAN,
                          RECOVERY_ACTION_REDUCE_LR, 0.9f);

    const recovery_episode_t* ptrs[20];
    for (int i = 0; i < 20; i++) ptrs[i] = &episodes[i];

    semantic_rule_t rule = consolidation_create_rule(consolidation, ptrs, 20);
    consolidation_add_rule(consolidation, &rule);

    // ACT
    error_pattern_t pattern;
    pattern.type = ERROR_TYPE_NAN;
    pattern.layer_id = 5;

    semantic_rule_t* retrieved = recovery_consolidation_get_rule(consolidation, &pattern);

    // ASSERT
    ASSERT_NE(retrieved, nullptr);
    EXPECT_EQ(retrieved->action, RECOVERY_ACTION_REDUCE_LR);
    EXPECT_NEAR(retrieved->success_rate, 0.9f, 0.01f);
}

/**
 * @test Rule capacity limits
 */
TEST_F(RecoveryConsolidationTest, RuleCapacityLimit) {
    // ARRANGE
    consolidation_config_t config = recovery_consolidation_default_config();
    config.max_rules = 10;
    consolidation = consolidation_create_custom(&config);

    // Create 15 different rules
    for (int i = 0; i < 15; i++) {
        recovery_episode_t episodes[15];
        create_similar_episodes(episodes, 15,
            (error_type_t)(ERROR_TYPE_NAN + i),
            RECOVERY_ACTION_REDUCE_LR, 0.8f);

        const recovery_episode_t* ptrs[15];
        for (int j = 0; j < 15; j++) ptrs[j] = &episodes[j];

        semantic_rule_t rule = consolidation_create_rule(consolidation, ptrs, 15);
        consolidation_add_rule(consolidation, &rule);
    }

    // ASSERT
    // Should cap at max_rules (10)
    EXPECT_LE(consolidation_get_rule_count(consolidation), 10);
}

//=============================================================================
// Test 7: Consolidation Process
//=============================================================================

/**
 * @test Run consolidation process
 *
 * WHAT: Test full consolidation pipeline
 * WHY:  Verify episodes → patterns → rules works end-to-end
 * HOW:  Add 25 episodes, run consolidation, check rules created
 */
TEST_F(RecoveryConsolidationTest, RunConsolidation) {
    // ARRANGE
    consolidation = recovery_consolidation_create();

    // Add 25 similar episodes
    for (int i = 0; i < 25; i++) {
        recovery_episode_t episode = create_test_episode(
            ERROR_TYPE_NAN, 5, RECOVERY_ACTION_REDUCE_LR,
            (i < 22),  // 22/25 success = 88%
            (i < 22) ? 0.8f : -0.4f
        );
        recovery_consolidation_add_episode(consolidation, &episode);
    }

    // ACT
    recovery_consolidation_run(consolidation);

    // ASSERT
    // Should create at least one rule
    EXPECT_GT(consolidation_get_rule_count(consolidation), 0);
    // Episodes should be consumed
    EXPECT_EQ(consolidation_get_episodes_pending(consolidation), 0);
}

/**
 * @test Consolidation with mixed error types
 */
TEST_F(RecoveryConsolidationTest, ConsolidationMixedErrors) {
    // ARRANGE
    consolidation = recovery_consolidation_create();

    // Add episodes for NaN errors
    for (int i = 0; i < 15; i++) {
        recovery_episode_t episode = create_test_episode(
            ERROR_TYPE_NAN, 5, RECOVERY_ACTION_REDUCE_LR, true
        );
        recovery_consolidation_add_episode(consolidation, &episode);
    }

    // Add episodes for overflow errors
    for (int i = 0; i < 15; i++) {
        recovery_episode_t episode = create_test_episode(
            ERROR_TYPE_OVERFLOW, 3, RECOVERY_ACTION_RELOAD_CHECKPOINT, true
        );
        recovery_consolidation_add_episode(consolidation, &episode);
    }

    // ACT
    recovery_consolidation_run(consolidation);

    // ASSERT
    // Should create 2 rules (one per error pattern)
    EXPECT_EQ(consolidation_get_rule_count(consolidation), 2);
}

/**
 * @test Consolidation status tracking
 */
TEST_F(RecoveryConsolidationTest, ConsolidationStatus) {
    // ARRANGE
    consolidation = recovery_consolidation_create();

    // Add episodes
    for (int i = 0; i < 20; i++) {
        recovery_episode_t episode = create_test_episode(
            ERROR_TYPE_NAN, 5, RECOVERY_ACTION_REDUCE_LR, true
        );
        recovery_consolidation_add_episode(consolidation, &episode);
    }

    EXPECT_FALSE(consolidation_is_active(consolidation));

    // ACT
    recovery_consolidation_run(consolidation);

    // ASSERT
    EXPECT_FALSE(consolidation_is_active(consolidation));  // Should finish
}

//=============================================================================
// Test 8: Background Consolidation
//=============================================================================

/**
 * @test Start background consolidation thread
 */
TEST_F(RecoveryConsolidationTest, BackgroundConsolidationStart) {
    // ARRANGE
    consolidation_config_t config = recovery_consolidation_default_config();
    config.enable_background_consolidation = true;
    consolidation = consolidation_create_custom(&config);

    // ACT
    bool result = consolidation_start_background(consolidation);

    // ASSERT
    EXPECT_TRUE(result);
    EXPECT_TRUE(consolidation_is_background_running(consolidation));

    // Cleanup
    consolidation_stop_background(consolidation);
}

/**
 * @test Stop background consolidation
 */
TEST_F(RecoveryConsolidationTest, BackgroundConsolidationStop) {
    // ARRANGE
    consolidation_config_t config = recovery_consolidation_default_config();
    config.enable_background_consolidation = true;
    consolidation = consolidation_create_custom(&config);

    consolidation_start_background(consolidation);
    ASSERT_TRUE(consolidation_is_background_running(consolidation));

    // ACT
    consolidation_stop_background(consolidation);

    // ASSERT
    EXPECT_FALSE(consolidation_is_background_running(consolidation));
}

//=============================================================================
// Test 9: Error Conditions
//=============================================================================

/**
 * @test NULL parameter handling across all functions
 */
TEST_F(RecoveryConsolidationTest, NullParameterHandling) {
    // All these should handle NULL gracefully without crashing
    EXPECT_EQ(consolidation_get_rule_count(nullptr), 0);
    EXPECT_EQ(consolidation_get_episodes_pending(nullptr), 0);
    EXPECT_EQ(consolidation_get_pattern_count(nullptr), 0);
    EXPECT_FALSE(consolidation_is_active(nullptr));
    EXPECT_FALSE(consolidation_is_background_running(nullptr));
    EXPECT_EQ(recovery_consolidation_get_rule(nullptr, nullptr), nullptr);

    recovery_consolidation_run(nullptr);
    SUCCEED();
}

//=============================================================================
// Test 10: Statistics and Reporting
//=============================================================================

/**
 * @test Get consolidation statistics
 */
TEST_F(RecoveryConsolidationTest, GetStatistics) {
    // ARRANGE
    consolidation = recovery_consolidation_create();

    // Add and consolidate episodes
    for (int i = 0; i < 20; i++) {
        recovery_episode_t episode = create_test_episode(
            ERROR_TYPE_NAN, 5, RECOVERY_ACTION_REDUCE_LR, true
        );
        recovery_consolidation_add_episode(consolidation, &episode);
    }
    recovery_consolidation_run(consolidation);

    // ACT
    consolidation_stats_t stats;
    bool result = recovery_consolidation_get_stats(consolidation, &stats);

    // ASSERT
    EXPECT_TRUE(result);
    EXPECT_EQ(stats.total_episodes_processed, 20);
    EXPECT_GT(stats.rules_created, 0);
    EXPECT_EQ(stats.consolidation_runs, 1);
}

/**
 * @test Statistics with NULL parameters
 */
TEST_F(RecoveryConsolidationTest, GetStatisticsNull) {
    // ARRANGE
    consolidation = recovery_consolidation_create();
    consolidation_stats_t stats;

    // ACT & ASSERT
    EXPECT_FALSE(recovery_consolidation_get_stats(nullptr, &stats));
    EXPECT_FALSE(recovery_consolidation_get_stats(consolidation, nullptr));
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
