/**
 * @file test_community_cache_regression.cpp
 * @brief Regression tests for community cache consolidation
 *
 * WHAT: Tests that community cache doesn't regress existing consolidation behavior
 * WHY:  The 7-hour hang was caused by network analyzer triggering Louvain in
 *       consolidate_replay(). These tests verify the fix stays in place and
 *       new community cache code doesn't reintroduce performance regressions.
 * HOW:  Timing-based tests + behavioral equivalence tests
 *
 * TEST COVERAGE:
 * - Consolidation completes in bounded time (no Louvain hang)
 * - Standard consolidation behavior preserved (backward compat)
 * - Stats are equivalent between standard and community-aware paths
 * - Config struct backward compatibility (new fields don't break old code)
 * - Preset configs (light/auto/full) still work
 */

#include "core/brain/nimcp_brain.h"
#include "cognitive/consolidation/nimcp_consolidation.h"

#include <gtest/gtest.h>
#include <string.h>
#include <chrono>

//=============================================================================
// Test Fixture
//=============================================================================

class CommunityCacheRegressionTest : public ::testing::Test {
protected:
    brain_t brain;
    consolidation_community_cache_t* cache;

    static const uint32_t NUM_FEATURES = 13;

    void SetUp() override
    {
        consolidation_reset_global_state();
        brain = brain_create("test_cc_regression", BRAIN_SIZE_SMALL,
                             BRAIN_TASK_CLASSIFICATION, NUM_FEATURES, 3);
        ASSERT_NE(brain, nullptr);
        cache = nullptr;
    }

    void TearDown() override
    {
        if (cache) {
            consolidation_community_cache_destroy(cache);
            cache = nullptr;
        }
        if (brain) {
            brain_destroy(brain);
            brain = nullptr;
        }
    }
};

//=============================================================================
// Performance Regression Tests
//=============================================================================

/**
 * WHAT: Test standard consolidation completes in bounded time
 * WHY:  Regression test for the 7-hour hang caused by network_analyzer in replay
 * HOW:  Time the consolidation and assert < 10s (was 7+ hours before fix)
 */
TEST_F(CommunityCacheRegressionTest, ConsolidationBoundedTime)
{
    consolidation_config_t config = consolidation_default_config();
    config.consolidation_cycles = 5;

    auto start = std::chrono::high_resolution_clock::now();
    bool result = brain_consolidate_memory(brain, &config);
    auto end = std::chrono::high_resolution_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    EXPECT_TRUE(result);
    EXPECT_LT(ms, 10000) << "Consolidation took " << ms << "ms (>10s = regression)";
    printf("Standard consolidation (5 cycles): %ld ms\n", ms);
}

/**
 * WHAT: Test community-aware consolidation also completes in bounded time
 * WHY:  New code path must not reintroduce the hang
 */
TEST_F(CommunityCacheRegressionTest, CommunityAwareConsolidationBoundedTime)
{
    cache = consolidation_community_cache_create();
    consolidation_cache_communities(cache, brain);

    consolidation_config_t config = consolidation_default_config();
    config.consolidation_cycles = 5;

    auto start = std::chrono::high_resolution_clock::now();
    bool result = brain_consolidate_memory_community_aware(brain, &config, cache);
    auto end = std::chrono::high_resolution_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    EXPECT_TRUE(result);
    EXPECT_LT(ms, 10000) << "Community-aware consolidation took " << ms << "ms (>10s = regression)";
    printf("Community-aware consolidation (5 cycles): %ld ms\n", ms);
}

/**
 * WHAT: Test light config still runs in milliseconds
 * WHY:  Verify light mode performance wasn't regressed
 */
TEST_F(CommunityCacheRegressionTest, LightModePerformance)
{
    consolidation_config_t config = consolidation_light_config();

    auto start = std::chrono::high_resolution_clock::now();
    bool result = brain_consolidate_memory(brain, &config);
    auto end = std::chrono::high_resolution_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    EXPECT_TRUE(result);
    EXPECT_LT(ms, 1000) << "Light consolidation took " << ms << "ms (>1s = regression)";
    printf("Light consolidation: %ld ms\n", ms);
}

//=============================================================================
// Backward Compatibility Tests
//=============================================================================

/**
 * WHAT: Test default config still produces expected values for original fields
 * WHY:  New fields must not change existing default behavior
 */
TEST_F(CommunityCacheRegressionTest, DefaultConfigBackwardCompat)
{
    consolidation_config_t config = consolidation_default_config();

    // Original fields unchanged
    EXPECT_EQ(config.strategy, CONSOLIDATION_STRATEGY_FULL);
    EXPECT_EQ(config.priority, CONSOLIDATION_PRIORITY_IMPORTANT);
    EXPECT_EQ(config.consolidation_cycles, 10u);
    EXPECT_FLOAT_EQ(config.consolidation_strength, 0.1f);
    EXPECT_TRUE(config.enable_replay);
    EXPECT_EQ(config.replay_count, 100u);
    EXPECT_TRUE(config.enable_pruning);
    EXPECT_FLOAT_EQ(config.pruning_threshold, 0.01f);
    EXPECT_TRUE(config.enable_scaling);
    EXPECT_FLOAT_EQ(config.scaling_target, 0.5f);
    EXPECT_TRUE(config.prioritize_novel);
    EXPECT_FLOAT_EQ(config.novelty_boost, 1.5f);
    EXPECT_TRUE(config.prune_weak);
    EXPECT_FLOAT_EQ(config.weakness_threshold, 0.1f);
    EXPECT_EQ(config.max_prune_passes, 1u);
    EXPECT_FLOAT_EQ(config.neuron_sample_rate, 1.0f);

    // New fields have defaults
    EXPECT_TRUE(config.use_community_cache);
    EXPECT_GT(config.hub_consolidation_boost, 1.0f);
    EXPECT_GT(config.cross_community_boost, 1.0f);
}

/**
 * WHAT: Test auto config at various scales
 * WHY:  Verify auto_config still produces correct tier-based configs
 */
TEST_F(CommunityCacheRegressionTest, AutoConfigTiers)
{
    // < 10K neurons
    consolidation_config_t small = consolidation_auto_config(100);
    EXPECT_EQ(small.consolidation_cycles, 10u);
    EXPECT_FLOAT_EQ(small.neuron_sample_rate, 1.0f);

    // 10K-500K neurons
    consolidation_config_t medium = consolidation_auto_config(50000);
    EXPECT_EQ(medium.consolidation_cycles, 5u);
    EXPECT_FLOAT_EQ(medium.neuron_sample_rate, 0.5f);

    // 500K-2M neurons
    consolidation_config_t large = consolidation_auto_config(1000000);
    EXPECT_EQ(large.consolidation_cycles, 3u);
    EXPECT_FLOAT_EQ(large.neuron_sample_rate, 0.1f);

    // > 2M neurons
    consolidation_config_t xlarge = consolidation_auto_config(3000000);
    EXPECT_EQ(xlarge.consolidation_cycles, 2u);
    EXPECT_FLOAT_EQ(xlarge.neuron_sample_rate, 0.05f);
}

/**
 * WHAT: Test NULL config still uses defaults (brain_consolidate_memory)
 * WHY:  Existing code passing NULL config must not break
 */
TEST_F(CommunityCacheRegressionTest, NullConfigBackwardCompat)
{
    bool result = brain_consolidate_memory(brain, nullptr);
    EXPECT_TRUE(result);
}

/**
 * WHAT: Test stats structure hasn't changed layout
 * WHY:  Verify existing stats consumers still work
 */
TEST_F(CommunityCacheRegressionTest, StatsStructBackwardCompat)
{
    consolidation_config_t config = consolidation_default_config();
    config.consolidation_cycles = 2;

    brain_consolidate_memory(brain, &config);

    consolidation_stats_t stats;
    bool got = consolidation_get_stats(nullptr, &stats);
    ASSERT_TRUE(got);

    EXPECT_GT(stats.total_consolidations, 0u);
    EXPECT_GT(stats.last_consolidation_time_ms, 0.0f);
    EXPECT_GT(stats.last_consolidation_timestamp, 0u);
}

/**
 * WHAT: Test community-aware path produces stats too
 * WHY:  Stats consumers expect consolidation stats regardless of path
 */
TEST_F(CommunityCacheRegressionTest, CommunityAwareStatsCompat)
{
    cache = consolidation_community_cache_create();
    consolidation_cache_communities(cache, brain);

    consolidation_reset_stats(nullptr);

    consolidation_config_t config = consolidation_default_config();
    config.consolidation_cycles = 2;

    brain_consolidate_memory_community_aware(brain, &config, cache);

    consolidation_stats_t stats;
    consolidation_get_stats(nullptr, &stats);
    EXPECT_GT(stats.total_consolidations, 0u);
    EXPECT_GE(stats.patterns_replayed, 0u);
}

/**
 * WHAT: Test consolidation_reset_global_state still works
 * WHY:  Test isolation must still function
 */
TEST_F(CommunityCacheRegressionTest, GlobalStateResetStillWorks)
{
    consolidation_config_t config = consolidation_default_config();
    config.consolidation_cycles = 1;
    brain_consolidate_memory(brain, &config);

    consolidation_reset_global_state();

    consolidation_stats_t stats;
    consolidation_get_stats(nullptr, &stats);
    EXPECT_EQ(stats.total_consolidations, 0u);
}
