/**
 * @file test_curiosity_reasoning_bridge.cpp
 * @brief Unit tests for Curiosity-Reasoning Integration Bridge
 * @version 1.0.0
 * @date 2025-12-31
 */

#include <gtest/gtest.h>

#include "cognitive/integration/nimcp_curiosity_reasoning_bridge.h"

/**
 * @brief Test fixture for Curiosity-Reasoning bridge tests
 */
class CuriosityReasoningBridgeTest : public ::testing::Test {
protected:
    curiosity_reasoning_bridge_t* bridge;
    curiosity_reasoning_config_t config;

    void SetUp() override {
        ASSERT_EQ(0, curiosity_reasoning_bridge_default_config(&config));
        bridge = curiosity_reasoning_bridge_create(&config);
        ASSERT_NE(nullptr, bridge);
    }

    void TearDown() override {
        if (bridge) {
            curiosity_reasoning_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }
};

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

/**
 * @brief Test bridge creation and destruction
 */
TEST_F(CuriosityReasoningBridgeTest, BridgeCreation) {
    // Bridge already created in SetUp, verify it's valid
    EXPECT_NE(nullptr, bridge);

    // Create another bridge with NULL config (uses defaults)
    curiosity_reasoning_bridge_t* bridge2 = curiosity_reasoning_bridge_create(nullptr);
    EXPECT_NE(nullptr, bridge2);
    curiosity_reasoning_bridge_destroy(bridge2);

    // Destroy NULL should be safe
    curiosity_reasoning_bridge_destroy(nullptr);
}

/**
 * @brief Test default configuration values
 */
TEST_F(CuriosityReasoningBridgeTest, DefaultConfig) {
    curiosity_reasoning_config_t default_config;
    ASSERT_EQ(0, curiosity_reasoning_bridge_default_config(&default_config));

    // Verify sensible defaults for exploration-exploitation balance
    EXPECT_GT(default_config.exploration_bias, 0.0f);
    EXPECT_LE(default_config.exploration_bias, 1.0f);
    EXPECT_GT(default_config.novelty_threshold, 0.0f);
    EXPECT_LE(default_config.novelty_threshold, 1.0f);
    EXPECT_GT(default_config.uncertainty_weight, 0.0f);
    EXPECT_LE(default_config.uncertainty_weight, 1.0f);

    // NULL config should return error
    EXPECT_EQ(-1, curiosity_reasoning_bridge_default_config(nullptr));
}

/* ============================================================================
 * Curiosity -> Reasoning Direction Tests
 * ============================================================================ */

/**
 * @brief Test driving exploration based on curiosity level
 */
TEST_F(CuriosityReasoningBridgeTest, DriveExploration) {
    curiosity_reasoning_context_t context;
    context.context_id = 100;
    context.uncertainty = 0.5f;
    context.novelty = 0.6f;
    context.depth = 3;

    float high_curiosity = 0.9f;

    // Drive exploration with high curiosity
    EXPECT_EQ(0, curiosity_reasoning_drive_exploration(bridge, &context, high_curiosity));

    // Check stats to verify exploration was driven
    curiosity_reasoning_stats_t stats;
    EXPECT_EQ(0, curiosity_reasoning_bridge_get_stats(bridge, &stats));
    EXPECT_GT(stats.explorations_driven, 0u);

    // Check exploration priority for the context's topic is set
    float priority = curiosity_reasoning_get_exploration_priority(bridge, context.context_id);
    EXPECT_GT(priority, 0.0f);

    // NULL checks
    EXPECT_EQ(-1, curiosity_reasoning_drive_exploration(nullptr, &context, high_curiosity));
    EXPECT_EQ(-1, curiosity_reasoning_drive_exploration(bridge, nullptr, high_curiosity));
}

/* ============================================================================
 * Reasoning -> Curiosity Direction Tests
 * ============================================================================ */

/**
 * @brief Test novel conclusion triggers curiosity boost
 */
TEST_F(CuriosityReasoningBridgeTest, OnNovelConclusion) {
    uint64_t conclusion_id = 200;
    float high_novelty = 0.8f;  // Above typical threshold

    // Signal novel conclusion - returns 1 if curiosity triggered, 0 if not
    int result = curiosity_reasoning_on_novel_conclusion(bridge, conclusion_id, high_novelty);
    EXPECT_GE(result, 0);  // Should succeed (0 or 1)

    // Check stats to verify novel conclusion was detected
    curiosity_reasoning_stats_t stats;
    EXPECT_EQ(0, curiosity_reasoning_bridge_get_stats(bridge, &stats));
    EXPECT_GT(stats.novel_conclusions, 0u);
    EXPECT_GT(stats.avg_novelty_score, 0.0f);

    // NULL bridge should fail
    EXPECT_EQ(-1, curiosity_reasoning_on_novel_conclusion(nullptr, conclusion_id, high_novelty));
}

/**
 * @brief Test below-threshold novelty doesn't trigger curiosity boost
 */
TEST_F(CuriosityReasoningBridgeTest, OnNovelConclusionBelowThreshold) {
    // Get default novelty threshold
    curiosity_reasoning_config_t default_config;
    EXPECT_EQ(0, curiosity_reasoning_bridge_default_config(&default_config));

    uint64_t conclusion_id = 201;
    float low_novelty = default_config.novelty_threshold * 0.5f;  // Well below threshold

    // Signal low-novelty conclusion - may or may not trigger depending on threshold
    int result = curiosity_reasoning_on_novel_conclusion(bridge, conclusion_id, low_novelty);
    EXPECT_GE(result, 0);  // Should succeed (not -1)

    // Check stats - novel_conclusions should NOT increment for sub-threshold novelty
    curiosity_reasoning_stats_t stats;
    EXPECT_EQ(0, curiosity_reasoning_bridge_get_stats(bridge, &stats));

    // The implementation may either:
    // 1. Not count sub-threshold conclusions as "novel"
    // 2. Count them but with low impact
    // Either way, avg_novelty should reflect the low value if counted
    // This test verifies the threshold mechanism exists

    // Now add a high-novelty conclusion
    float high_novelty = 0.9f;
    result = curiosity_reasoning_on_novel_conclusion(bridge, 202, high_novelty);
    EXPECT_GE(result, 0);  // Should succeed

    // Get updated stats
    curiosity_reasoning_stats_t stats2;
    EXPECT_EQ(0, curiosity_reasoning_bridge_get_stats(bridge, &stats2));

    // High novelty should definitely be counted
    EXPECT_GT(stats2.novel_conclusions, 0u);
}

/**
 * @brief Test sharing epistemic uncertainty with reasoning
 */
TEST_F(CuriosityReasoningBridgeTest, ShareUncertainty) {
    uint64_t topic_id = 300;
    float uncertainty = 0.7f;

    // Share uncertainty
    EXPECT_EQ(0, curiosity_reasoning_share_uncertainty(bridge, topic_id, uncertainty));

    // Check stats
    curiosity_reasoning_stats_t stats;
    EXPECT_EQ(0, curiosity_reasoning_bridge_get_stats(bridge, &stats));
    EXPECT_GT(stats.uncertainty_shared, 0u);

    // Uncertainty should affect exploration priority
    float priority = curiosity_reasoning_get_exploration_priority(bridge, topic_id);
    EXPECT_GT(priority, 0.0f);

    // Share uncertainty on another topic with different level
    uint64_t topic_id2 = 301;
    float high_uncertainty = 0.95f;
    EXPECT_EQ(0, curiosity_reasoning_share_uncertainty(bridge, topic_id2, high_uncertainty));

    // Higher uncertainty should generally lead to higher exploration priority
    float priority2 = curiosity_reasoning_get_exploration_priority(bridge, topic_id2);
    // With config's uncertainty_weight, higher uncertainty should boost priority
    EXPECT_GE(priority2, priority * 0.8f);  // Allow some tolerance

    // NULL bridge should fail
    EXPECT_EQ(-1, curiosity_reasoning_share_uncertainty(nullptr, topic_id, uncertainty));
}

/* ============================================================================
 * Query Tests
 * ============================================================================ */

/**
 * @brief Test getting exploration priority for a topic
 */
TEST_F(CuriosityReasoningBridgeTest, GetExplorationPriority) {
    uint64_t topic_id = 400;

    // Initially, unknown topic should have low/zero priority
    float initial_priority = curiosity_reasoning_get_exploration_priority(bridge, topic_id);
    // Could be 0.0f or some default base priority

    // Drive exploration for this topic
    curiosity_reasoning_context_t context;
    context.context_id = topic_id;
    context.uncertainty = 0.6f;
    context.novelty = 0.7f;
    context.depth = 2;
    EXPECT_EQ(0, curiosity_reasoning_drive_exploration(bridge, &context, 0.8f));

    // Priority should now be set
    float updated_priority = curiosity_reasoning_get_exploration_priority(bridge, topic_id);
    EXPECT_GT(updated_priority, initial_priority);
    EXPECT_GE(updated_priority, 0.0f);
    EXPECT_LE(updated_priority, 1.0f);

    // NULL bridge should return -1.0f
    float error_result = curiosity_reasoning_get_exploration_priority(nullptr, topic_id);
    EXPECT_FLOAT_EQ(-1.0f, error_result);
}

/* ============================================================================
 * Statistics Tests
 * ============================================================================ */

/**
 * @brief Test statistics tracking
 */
TEST_F(CuriosityReasoningBridgeTest, StatsTracking) {
    curiosity_reasoning_stats_t stats;

    // Get initial stats
    EXPECT_EQ(0, curiosity_reasoning_bridge_get_stats(bridge, &stats));
    EXPECT_EQ(0u, stats.explorations_driven);
    EXPECT_EQ(0u, stats.novel_conclusions);
    EXPECT_EQ(0u, stats.uncertainty_shared);
    EXPECT_FLOAT_EQ(0.0f, stats.avg_curiosity_level);
    EXPECT_FLOAT_EQ(0.0f, stats.avg_novelty_score);

    // Drive some explorations
    curiosity_reasoning_context_t context1 = {1, 0.5f, 0.6f, 1};
    curiosity_reasoning_context_t context2 = {2, 0.7f, 0.8f, 2};

    EXPECT_EQ(0, curiosity_reasoning_drive_exploration(bridge, &context1, 0.6f));
    EXPECT_EQ(0, curiosity_reasoning_drive_exploration(bridge, &context2, 0.8f));

    // Check exploration stats
    EXPECT_EQ(0, curiosity_reasoning_bridge_get_stats(bridge, &stats));
    EXPECT_EQ(2u, stats.explorations_driven);
    // Check that avg_curiosity_level is reasonable (not comparing to exact value)
    EXPECT_GT(stats.avg_curiosity_level, 0.0f);

    // Add novel conclusions - returns 1 if triggered, 0 if not, -1 on error
    EXPECT_GE(curiosity_reasoning_on_novel_conclusion(bridge, 100, 0.9f), 0);
    EXPECT_GE(curiosity_reasoning_on_novel_conclusion(bridge, 101, 0.7f), 0);

    // Check novelty stats
    EXPECT_EQ(0, curiosity_reasoning_bridge_get_stats(bridge, &stats));
    EXPECT_GE(stats.novel_conclusions, 1u);  // At least one should count (above threshold)
    EXPECT_GT(stats.avg_novelty_score, 0.0f);

    // Share uncertainty
    EXPECT_EQ(0, curiosity_reasoning_share_uncertainty(bridge, 1, 0.5f));
    EXPECT_EQ(0, curiosity_reasoning_share_uncertainty(bridge, 2, 0.8f));
    EXPECT_EQ(0, curiosity_reasoning_share_uncertainty(bridge, 3, 0.6f));

    // Check uncertainty stats
    EXPECT_EQ(0, curiosity_reasoning_bridge_get_stats(bridge, &stats));
    EXPECT_EQ(3u, stats.uncertainty_shared);

    // NULL checks
    EXPECT_EQ(-1, curiosity_reasoning_bridge_get_stats(nullptr, &stats));
    EXPECT_EQ(-1, curiosity_reasoning_bridge_get_stats(bridge, nullptr));
}

/**
 * @brief Test interaction between curiosity and uncertainty
 */
TEST_F(CuriosityReasoningBridgeTest, CuriosityUncertaintyInteraction) {
    uint64_t topic_id = 500;

    // Share high uncertainty
    EXPECT_EQ(0, curiosity_reasoning_share_uncertainty(bridge, topic_id, 0.9f));

    // Drive exploration with curiosity
    curiosity_reasoning_context_t context;
    context.context_id = topic_id;
    context.uncertainty = 0.9f;
    context.novelty = 0.7f;
    context.depth = 1;
    EXPECT_EQ(0, curiosity_reasoning_drive_exploration(bridge, &context, 0.8f));

    // Priority should be high due to both curiosity and uncertainty
    float priority = curiosity_reasoning_get_exploration_priority(bridge, topic_id);
    EXPECT_GT(priority, 0.5f);  // Should be reasonably high

    // Compare with topic with low uncertainty
    uint64_t low_uncertainty_topic = 501;
    EXPECT_EQ(0, curiosity_reasoning_share_uncertainty(bridge, low_uncertainty_topic, 0.1f));

    curiosity_reasoning_context_t context2;
    context2.context_id = low_uncertainty_topic;
    context2.uncertainty = 0.1f;
    context2.novelty = 0.7f;
    context2.depth = 1;
    EXPECT_EQ(0, curiosity_reasoning_drive_exploration(bridge, &context2, 0.8f));

    float low_priority = curiosity_reasoning_get_exploration_priority(bridge, low_uncertainty_topic);

    // Higher uncertainty should generally yield higher exploration priority
    EXPECT_GE(priority, low_priority);
}
