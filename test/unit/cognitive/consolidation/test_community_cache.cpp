/**
 * @file test_community_cache.cpp
 * @brief Unit tests for consolidation community cache
 *
 * WHAT: Tests community cache lifecycle, population, queries, and invalidation
 * WHY:  Community cache enables O(1) topology queries during consolidation replay
 *       without triggering expensive O(N log N) Louvain detection
 * HOW:  GTest fixture with brain creation, cache lifecycle, and query verification
 *
 * TEST COVERAGE:
 * - Cache creation and destruction
 * - Cache population via consolidation_cache_communities()
 * - Community query (consolidation_cache_get_community)
 * - Hub query (consolidation_cache_is_hub)
 * - Cache validity checks
 * - Cache invalidation
 * - NULL safety for all functions
 * - Config defaults for community-aware fields
 * - Light/auto/full config presets
 */

#include "core/brain/nimcp_brain.h"
#include "cognitive/consolidation/nimcp_consolidation.h"

#include <gtest/gtest.h>
#include <string.h>
#include <chrono>

//=============================================================================
// Test Fixture
//=============================================================================

class CommunityCacheTest : public ::testing::Test {
protected:
    brain_t brain;
    consolidation_community_cache_t* cache;

    static const uint32_t NUM_FEATURES = 13;

    void SetUp() override
    {
        consolidation_reset_global_state();
        brain = brain_create("test_community_cache", BRAIN_SIZE_SMALL,
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
// Cache Lifecycle Tests
//=============================================================================

/**
 * WHAT: Test cache creation returns valid pointer
 * WHY:  Verify basic allocation succeeds
 */
TEST_F(CommunityCacheTest, CreateReturnsNonNull)
{
    cache = consolidation_community_cache_create();
    ASSERT_NE(cache, nullptr);
    EXPECT_FALSE(cache->is_valid);
    EXPECT_EQ(cache->num_neurons, 0u);
    EXPECT_EQ(cache->num_communities, 0u);
    EXPECT_EQ(cache->num_hubs, 0u);
}

/**
 * WHAT: Test cache destruction with NULL is safe
 * WHY:  Prevent crashes on NULL pointer
 */
TEST_F(CommunityCacheTest, DestroyNullSafe)
{
    consolidation_community_cache_destroy(nullptr);
    // Should not crash
}

/**
 * WHAT: Test cache destruction frees cleanly
 * WHY:  Prevent memory leaks
 */
TEST_F(CommunityCacheTest, DestroyAfterCreate)
{
    cache = consolidation_community_cache_create();
    ASSERT_NE(cache, nullptr);
    consolidation_community_cache_destroy(cache);
    cache = nullptr;  // Prevent double-free in TearDown
}

//=============================================================================
// Cache Population Tests
//=============================================================================

/**
 * WHAT: Test populating cache from brain
 * WHY:  Verify community detection runs and results are stored
 */
TEST_F(CommunityCacheTest, CacheCommunitiesFromBrain)
{
    cache = consolidation_community_cache_create();
    ASSERT_NE(cache, nullptr);

    bool result = consolidation_cache_communities(cache, brain);
    EXPECT_TRUE(result);
    EXPECT_TRUE(cache->is_valid);
    EXPECT_GT(cache->num_neurons, 0u);
    EXPECT_GT(cache->num_communities, 0u);
    EXPECT_NE(cache->community_ids, nullptr);
    EXPECT_GT(cache->cache_timestamp_ms, 0u);
}

/**
 * WHAT: Test cache population with NULL cache
 * WHY:  Verify error handling
 */
TEST_F(CommunityCacheTest, CacheCommunitiesNullCache)
{
    bool result = consolidation_cache_communities(nullptr, brain);
    EXPECT_FALSE(result);
}

/**
 * WHAT: Test cache population with NULL brain
 * WHY:  Verify error handling
 */
TEST_F(CommunityCacheTest, CacheCommunitiesNullBrain)
{
    cache = consolidation_community_cache_create();
    bool result = consolidation_cache_communities(cache, nullptr);
    EXPECT_FALSE(result);
    EXPECT_FALSE(cache->is_valid);
}

/**
 * WHAT: Test repopulating cache (second call overwrites first)
 * WHY:  Verify memory is freed properly on re-populate
 */
TEST_F(CommunityCacheTest, RepopulateCache)
{
    cache = consolidation_community_cache_create();
    ASSERT_NE(cache, nullptr);

    // First population
    bool result1 = consolidation_cache_communities(cache, brain);
    EXPECT_TRUE(result1);
    uint32_t first_communities = cache->num_communities;

    // Second population (should free old data, repopulate)
    bool result2 = consolidation_cache_communities(cache, brain);
    EXPECT_TRUE(result2);
    EXPECT_TRUE(cache->is_valid);
    // Same brain, so should get same number of communities
    EXPECT_EQ(cache->num_communities, first_communities);
}

//=============================================================================
// Cache Query Tests
//=============================================================================

/**
 * WHAT: Test querying community ID for valid neuron
 * WHY:  Verify O(1) lookup works correctly
 */
TEST_F(CommunityCacheTest, GetCommunityValid)
{
    cache = consolidation_community_cache_create();
    consolidation_cache_communities(cache, brain);
    ASSERT_TRUE(cache->is_valid);

    uint32_t community = consolidation_cache_get_community(cache, 0);
    EXPECT_NE(community, UINT32_MAX);
    EXPECT_LT(community, cache->num_communities);
}

/**
 * WHAT: Test querying community for out-of-range neuron
 * WHY:  Verify bounds checking
 */
TEST_F(CommunityCacheTest, GetCommunityOutOfRange)
{
    cache = consolidation_community_cache_create();
    consolidation_cache_communities(cache, brain);

    uint32_t community = consolidation_cache_get_community(cache, UINT32_MAX);
    EXPECT_EQ(community, UINT32_MAX);
}

/**
 * WHAT: Test querying community with NULL cache
 * WHY:  Verify NULL safety
 */
TEST_F(CommunityCacheTest, GetCommunityNullCache)
{
    uint32_t community = consolidation_cache_get_community(nullptr, 0);
    EXPECT_EQ(community, UINT32_MAX);
}

/**
 * WHAT: Test querying community with invalid (empty) cache
 * WHY:  Verify is_valid check
 */
TEST_F(CommunityCacheTest, GetCommunityInvalidCache)
{
    cache = consolidation_community_cache_create();
    // Don't populate — cache->is_valid = false

    uint32_t community = consolidation_cache_get_community(cache, 0);
    EXPECT_EQ(community, UINT32_MAX);
}

/**
 * WHAT: Test hub query function
 * WHY:  Verify hub lookup returns centrality and connector status
 */
TEST_F(CommunityCacheTest, IsHubQuery)
{
    cache = consolidation_community_cache_create();
    consolidation_cache_communities(cache, brain);

    // Query each hub (if any exist)
    for (uint32_t h = 0; h < cache->num_hubs && cache->hub_neuron_ids; h++) {
        float centrality = 0.0f;
        bool is_connector = false;
        bool is_hub = consolidation_cache_is_hub(
            cache, cache->hub_neuron_ids[h], &centrality, &is_connector);
        EXPECT_TRUE(is_hub);
        EXPECT_GE(centrality, 0.0f);
        EXPECT_LE(centrality, 1.0f);
    }
}

/**
 * WHAT: Test hub query for non-hub neuron
 * WHY:  Verify false return for non-hub neurons
 */
TEST_F(CommunityCacheTest, IsHubNonHubNeuron)
{
    cache = consolidation_community_cache_create();
    consolidation_cache_communities(cache, brain);

    // Use an unlikely neuron ID (UINT32_MAX - 1)
    float centrality = -1.0f;
    bool is_connector = true;
    bool is_hub = consolidation_cache_is_hub(cache, UINT32_MAX - 1, &centrality, &is_connector);
    EXPECT_FALSE(is_hub);
}

/**
 * WHAT: Test hub query with NULL output params
 * WHY:  Verify NULL output params are handled safely
 */
TEST_F(CommunityCacheTest, IsHubNullOutputs)
{
    cache = consolidation_community_cache_create();
    consolidation_cache_communities(cache, brain);

    if (cache->num_hubs > 0 && cache->hub_neuron_ids) {
        bool is_hub = consolidation_cache_is_hub(
            cache, cache->hub_neuron_ids[0], nullptr, nullptr);
        EXPECT_TRUE(is_hub);
    }
}

/**
 * WHAT: Test hub query with NULL cache
 * WHY:  Verify NULL safety
 */
TEST_F(CommunityCacheTest, IsHubNullCache)
{
    bool is_hub = consolidation_cache_is_hub(nullptr, 0, nullptr, nullptr);
    EXPECT_FALSE(is_hub);
}

//=============================================================================
// Cache Validity Tests
//=============================================================================

/**
 * WHAT: Test cache validity check on populated cache
 * WHY:  Verify valid cache returns true
 */
TEST_F(CommunityCacheTest, IsValidPopulated)
{
    cache = consolidation_community_cache_create();
    consolidation_cache_communities(cache, brain);

    bool valid = consolidation_community_cache_is_valid(cache, brain, 0);
    EXPECT_TRUE(valid);
}

/**
 * WHAT: Test cache validity check on empty cache
 * WHY:  Verify empty cache returns false
 */
TEST_F(CommunityCacheTest, IsValidEmpty)
{
    cache = consolidation_community_cache_create();

    bool valid = consolidation_community_cache_is_valid(cache, brain, 0);
    EXPECT_FALSE(valid);
}

/**
 * WHAT: Test cache validity with NULL cache
 * WHY:  Verify NULL safety
 */
TEST_F(CommunityCacheTest, IsValidNullCache)
{
    bool valid = consolidation_community_cache_is_valid(nullptr, brain, 0);
    EXPECT_FALSE(valid);
}

/**
 * WHAT: Test cache validity with age limit (should be valid when fresh)
 * WHY:  Verify age check works for fresh cache
 */
TEST_F(CommunityCacheTest, IsValidFreshWithAgeLimit)
{
    cache = consolidation_community_cache_create();
    consolidation_cache_communities(cache, brain);

    // Cache was just built, 60 second age limit should pass
    bool valid = consolidation_community_cache_is_valid(cache, brain, 60000);
    EXPECT_TRUE(valid);
}

//=============================================================================
// Cache Invalidation Tests
//=============================================================================

/**
 * WHAT: Test cache invalidation
 * WHY:  Verify invalidate marks cache as stale
 */
TEST_F(CommunityCacheTest, Invalidate)
{
    cache = consolidation_community_cache_create();
    consolidation_cache_communities(cache, brain);
    EXPECT_TRUE(cache->is_valid);

    consolidation_community_cache_invalidate(cache);
    EXPECT_FALSE(cache->is_valid);

    // Queries should fail on invalidated cache
    uint32_t community = consolidation_cache_get_community(cache, 0);
    EXPECT_EQ(community, UINT32_MAX);
}

/**
 * WHAT: Test invalidation with NULL cache
 * WHY:  Verify NULL safety
 */
TEST_F(CommunityCacheTest, InvalidateNullSafe)
{
    consolidation_community_cache_invalidate(nullptr);
    // Should not crash
}

//=============================================================================
// Config Defaults Tests
//=============================================================================

/**
 * WHAT: Test default config has community-aware fields set
 * WHY:  Verify new config fields have sensible defaults
 */
TEST_F(CommunityCacheTest, DefaultConfigCommunityFields)
{
    consolidation_config_t config = consolidation_default_config();

    EXPECT_TRUE(config.use_community_cache);
    EXPECT_FLOAT_EQ(config.hub_consolidation_boost, 1.5f);
    EXPECT_FLOAT_EQ(config.cross_community_boost, 1.3f);
}

/**
 * WHAT: Test light config has community-aware fields
 * WHY:  Light config should inherit defaults
 */
TEST_F(CommunityCacheTest, LightConfigCommunityFields)
{
    consolidation_config_t config = consolidation_light_config();

    EXPECT_TRUE(config.use_community_cache);
    EXPECT_GT(config.hub_consolidation_boost, 1.0f);
    EXPECT_GT(config.cross_community_boost, 1.0f);
}

/**
 * WHAT: Test auto config has community-aware fields
 * WHY:  Auto config should inherit defaults
 */
TEST_F(CommunityCacheTest, AutoConfigCommunityFields)
{
    consolidation_config_t config = consolidation_auto_config(100);

    EXPECT_TRUE(config.use_community_cache);
    EXPECT_GT(config.hub_consolidation_boost, 1.0f);
    EXPECT_GT(config.cross_community_boost, 1.0f);
}

//=============================================================================
// Community-Aware Consolidation Tests
//=============================================================================

/**
 * WHAT: Test community-aware consolidation with populated cache
 * WHY:  Verify the full pipeline works: cache → consolidate
 */
TEST_F(CommunityCacheTest, ConsolidateWithCache)
{
    cache = consolidation_community_cache_create();
    consolidation_cache_communities(cache, brain);

    consolidation_config_t config = consolidation_default_config();
    config.consolidation_cycles = 2;

    bool result = brain_consolidate_memory_community_aware(brain, &config, cache);
    EXPECT_TRUE(result);
}

/**
 * WHAT: Test community-aware consolidation with NULL cache falls back
 * WHY:  Verify graceful fallback to standard consolidation
 */
TEST_F(CommunityCacheTest, ConsolidateWithNullCache)
{
    consolidation_config_t config = consolidation_default_config();
    config.consolidation_cycles = 2;

    bool result = brain_consolidate_memory_community_aware(brain, &config, nullptr);
    EXPECT_TRUE(result);
}

/**
 * WHAT: Test community-aware consolidation with NULL brain
 * WHY:  Verify error handling
 */
TEST_F(CommunityCacheTest, ConsolidateWithNullBrain)
{
    cache = consolidation_community_cache_create();

    consolidation_config_t config = consolidation_default_config();
    bool result = brain_consolidate_memory_community_aware(nullptr, &config, cache);
    EXPECT_FALSE(result);
}

/**
 * WHAT: Test community-aware consolidation updates stats
 * WHY:  Verify statistics are tracked during community-aware consolidation
 */
TEST_F(CommunityCacheTest, ConsolidateWithCacheUpdatesStats)
{
    cache = consolidation_community_cache_create();
    consolidation_cache_communities(cache, brain);

    consolidation_config_t config = consolidation_default_config();
    config.consolidation_cycles = 3;

    consolidation_reset_stats(nullptr);
    brain_consolidate_memory_community_aware(brain, &config, cache);

    consolidation_stats_t stats;
    consolidation_get_stats(nullptr, &stats);
    EXPECT_GT(stats.total_consolidations, 0u);
    EXPECT_GT(stats.last_consolidation_time_ms, 0.0f);
}

/**
 * WHAT: Test consolidation with community cache disabled in config
 * WHY:  Verify use_community_cache=false skips cache even when provided
 */
TEST_F(CommunityCacheTest, ConsolidateCacheDisabledInConfig)
{
    cache = consolidation_community_cache_create();
    consolidation_cache_communities(cache, brain);

    consolidation_config_t config = consolidation_default_config();
    config.consolidation_cycles = 2;
    config.use_community_cache = false;

    bool result = brain_consolidate_memory_community_aware(brain, &config, cache);
    EXPECT_TRUE(result);
}

/**
 * WHAT: Test brain_consolidate_memory still works (backward compat)
 * WHY:  Existing code using brain_consolidate_memory must not break
 */
TEST_F(CommunityCacheTest, BackwardCompatNoCache)
{
    consolidation_config_t config = consolidation_default_config();
    config.consolidation_cycles = 2;

    bool result = brain_consolidate_memory(brain, &config);
    EXPECT_TRUE(result);
}

/**
 * WHAT: Test modularity value is in expected range
 * WHY:  Verify community detection produces meaningful Q score
 */
TEST_F(CommunityCacheTest, ModularityRange)
{
    cache = consolidation_community_cache_create();
    consolidation_cache_communities(cache, brain);

    // Q is in [-0.5, 1.0] but typically >= 0 for real networks
    EXPECT_GE(cache->modularity, -0.5f);
    EXPECT_LE(cache->modularity, 1.0f);
}

/**
 * WHAT: Test all neurons have valid community assignments
 * WHY:  Verify community_ids array is fully populated
 */
TEST_F(CommunityCacheTest, AllNeuronsAssigned)
{
    cache = consolidation_community_cache_create();
    consolidation_cache_communities(cache, brain);
    ASSERT_TRUE(cache->is_valid);
    ASSERT_NE(cache->community_ids, nullptr);

    for (uint32_t i = 0; i < cache->num_neurons; i++) {
        EXPECT_LT(cache->community_ids[i], cache->num_communities)
            << "Neuron " << i << " has invalid community ID";
    }
}
