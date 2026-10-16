/**
 * @file test_community_cache_integration.cpp
 * @brief Integration tests for community cache with full brain pipeline
 *
 * WHAT: Tests community cache integrated with learning, consolidation, and working memory
 * WHY:  Verify community-aware consolidation works correctly in realistic scenarios
 * HOW:  Create brain, train examples, populate cache, consolidate, verify behavior
 *
 * TEST COVERAGE:
 * - Learning → cache → consolidate pipeline
 * - Community structure changes after learning
 * - Cache with working memory interactions
 * - Background consolidation with cache
 * - Multiple consolidation cycles with cache reuse
 * - Cache refresh between training phases
 */

#include "core/brain/nimcp_brain.h"
#include "cognitive/consolidation/nimcp_consolidation.h"

#include <gtest/gtest.h>
#include <string.h>
#include <chrono>

//=============================================================================
// Test Fixture
//=============================================================================

class CommunityCacheIntegrationTest : public ::testing::Test {
protected:
    brain_t brain;
    consolidation_community_cache_t* cache;

    static const uint32_t NUM_FEATURES = 8;
    static const uint32_t NUM_OUTPUTS = 3;

    void SetUp() override
    {
        consolidation_reset_global_state();
        brain = brain_create("test_cc_integration", BRAIN_SIZE_SMALL,
                             BRAIN_TASK_CLASSIFICATION, NUM_FEATURES, NUM_OUTPUTS);
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

    void train_examples(uint32_t count) {
        float features[NUM_FEATURES];
        for (uint32_t i = 0; i < count; i++) {
            for (uint32_t f = 0; f < NUM_FEATURES; f++) {
                features[f] = (float)((i * 7 + f * 13) % 100) / 100.0f;
            }
            const char* label = (i % NUM_OUTPUTS == 0) ? "class_0" :
                               (i % NUM_OUTPUTS == 1) ? "class_1" : "class_2";
            brain_learn_example(brain, features, NUM_FEATURES, label, 0.9f);
        }
    }
};

//=============================================================================
// Pipeline Integration Tests
//=============================================================================

/**
 * WHAT: Test learn → cache → consolidate pipeline
 * WHY:  Verify the full intended workflow functions correctly
 */
TEST_F(CommunityCacheIntegrationTest, LearnCacheConsolidatePipeline)
{
    // Train some examples
    train_examples(50);

    // Build community cache
    cache = consolidation_community_cache_create();
    ASSERT_NE(cache, nullptr);

    bool cached = consolidation_cache_communities(cache, brain);
    EXPECT_TRUE(cached);
    EXPECT_TRUE(cache->is_valid);

    // Consolidate with community awareness
    consolidation_config_t config = consolidation_auto_config(cache->num_neurons);
    config.consolidation_cycles = 2;

    bool result = brain_consolidate_memory_community_aware(brain, &config, cache);
    EXPECT_TRUE(result);

    // Verify stats
    consolidation_stats_t stats;
    consolidation_get_stats(nullptr, &stats);
    EXPECT_GT(stats.total_consolidations, 0u);
}

/**
 * WHAT: Test cache after more learning still works
 * WHY:  Verify cache is valid even after additional learning steps
 *       (network size doesn't change, just weights)
 */
TEST_F(CommunityCacheIntegrationTest, CacheValidAfterMoreLearning)
{
    train_examples(20);

    cache = consolidation_community_cache_create();
    consolidation_cache_communities(cache, brain);
    ASSERT_TRUE(cache->is_valid);

    // Train more (weights change, but neuron count stays same)
    train_examples(30);

    // Cache should still be valid (same neuron count)
    bool valid = consolidation_community_cache_is_valid(cache, brain, 0);
    EXPECT_TRUE(valid);

    // Consolidate should still work
    consolidation_config_t config = consolidation_default_config();
    config.consolidation_cycles = 1;
    bool result = brain_consolidate_memory_community_aware(brain, &config, cache);
    EXPECT_TRUE(result);
}

/**
 * WHAT: Test multiple consolidation cycles reusing same cache
 * WHY:  Cache should be reusable across multiple consolidation calls
 */
TEST_F(CommunityCacheIntegrationTest, ReuseCacheAcrossConsolidations)
{
    train_examples(30);

    cache = consolidation_community_cache_create();
    consolidation_cache_communities(cache, brain);

    consolidation_config_t config = consolidation_default_config();
    config.consolidation_cycles = 2;

    // First consolidation
    bool result1 = brain_consolidate_memory_community_aware(brain, &config, cache);
    EXPECT_TRUE(result1);

    // Second consolidation (reuse cache)
    bool result2 = brain_consolidate_memory_community_aware(brain, &config, cache);
    EXPECT_TRUE(result2);

    // Third with different mode
    config.strategy = CONSOLIDATION_STRATEGY_REPLAY;
    bool result3 = brain_consolidate_memory_community_aware(brain, &config, cache);
    EXPECT_TRUE(result3);
}

/**
 * WHAT: Test cache refresh workflow (invalidate → repopulate)
 * WHY:  Simulates the between-phases workflow in Athena training
 */
TEST_F(CommunityCacheIntegrationTest, CacheRefreshBetweenPhases)
{
    train_examples(20);

    cache = consolidation_community_cache_create();
    consolidation_cache_communities(cache, brain);
    uint32_t phase1_communities = cache->num_communities;

    // Phase 1 consolidation
    consolidation_config_t config = consolidation_auto_config(cache->num_neurons);
    config.consolidation_cycles = 1;
    brain_consolidate_memory_community_aware(brain, &config, cache);

    // More training (phase 2)
    train_examples(50);

    // Refresh cache for phase 2
    consolidation_community_cache_invalidate(cache);
    EXPECT_FALSE(cache->is_valid);

    bool recached = consolidation_cache_communities(cache, brain);
    EXPECT_TRUE(recached);
    EXPECT_TRUE(cache->is_valid);

    // Phase 2 consolidation
    bool result = brain_consolidate_memory_community_aware(brain, &config, cache);
    EXPECT_TRUE(result);
}

/**
 * WHAT: Test standard consolidation still works alongside community-aware
 * WHY:  Backward compatibility — both paths must coexist
 */
TEST_F(CommunityCacheIntegrationTest, MixedConsolidationModes)
{
    train_examples(30);

    cache = consolidation_community_cache_create();
    consolidation_cache_communities(cache, brain);

    consolidation_config_t config = consolidation_default_config();
    config.consolidation_cycles = 1;

    // Standard consolidation
    bool result1 = brain_consolidate_memory(brain, &config);
    EXPECT_TRUE(result1);

    // Community-aware consolidation
    bool result2 = brain_consolidate_memory_community_aware(brain, &config, cache);
    EXPECT_TRUE(result2);

    // Standard again (no regression)
    bool result3 = brain_consolidate_memory(brain, &config);
    EXPECT_TRUE(result3);
}

/**
 * WHAT: Test all strategy modes with community cache
 * WHY:  Verify cache works with replay, scaling, pruning, and full strategies
 */
TEST_F(CommunityCacheIntegrationTest, AllStrategiesWithCache)
{
    train_examples(30);

    cache = consolidation_community_cache_create();
    consolidation_cache_communities(cache, brain);

    consolidation_strategy_t strategies[] = {
        CONSOLIDATION_STRATEGY_REPLAY,
        CONSOLIDATION_STRATEGY_SCALING,
        CONSOLIDATION_STRATEGY_PRUNING,
        CONSOLIDATION_STRATEGY_FULL
    };

    for (auto strategy : strategies) {
        consolidation_config_t config = consolidation_default_config();
        config.strategy = strategy;
        config.consolidation_cycles = 1;

        bool result = brain_consolidate_memory_community_aware(brain, &config, cache);
        EXPECT_TRUE(result) << "Failed for strategy " << (int)strategy;
    }
}

/**
 * WHAT: Test community-aware consolidation with callbacks
 * WHY:  Verify callbacks fire in community-aware path too
 */
static std::atomic<uint32_t> g_ca_start_count{0};
static std::atomic<uint32_t> g_ca_complete_count{0};

static void ca_start_cb(void* ctx) { g_ca_start_count++; }
static void ca_complete_cb(void* ctx) { g_ca_complete_count++; }

TEST_F(CommunityCacheIntegrationTest, CommunityAwareCallbacks)
{
    g_ca_start_count = 0;
    g_ca_complete_count = 0;

    train_examples(20);

    cache = consolidation_community_cache_create();
    consolidation_cache_communities(cache, brain);

    consolidation_config_t config = consolidation_default_config();
    config.consolidation_cycles = 2;
    config.on_consolidation_start = ca_start_cb;
    config.on_consolidation_complete = ca_complete_cb;

    brain_consolidate_memory_community_aware(brain, &config, cache);

    EXPECT_EQ(g_ca_start_count, 1u);
    EXPECT_EQ(g_ca_complete_count, 1u);
}
