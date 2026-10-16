/**
 * @file test_community_cache_e2e.cpp
 * @brief End-to-end tests for community-aware consolidation pipeline
 *
 * WHAT: Tests the full training → cache → consolidate → predict pipeline
 * WHY:  Verify community-aware consolidation works in realistic end-to-end scenarios
 *       mimicking the Athena training workflow
 * HOW:  Multi-phase training with community cache refresh between phases
 *
 * TEST COVERAGE:
 * - Full training pipeline with community-aware consolidation
 * - Multi-phase training with cache refresh
 * - Mixed consolidation modes (light → auto → full)
 * - Prediction after community-aware consolidation
 * - Cache lifecycle across full brain lifecycle
 * - Performance bounds for full pipeline
 */

#include "core/brain/nimcp_brain.h"
#include "cognitive/consolidation/nimcp_consolidation.h"

#include <gtest/gtest.h>
#include <string.h>
#include <chrono>
#include <vector>
#include <cstdlib>

//=============================================================================
// Test Fixture
//=============================================================================

class CommunityCacheE2ETest : public ::testing::Test {
protected:
    brain_t brain;
    consolidation_community_cache_t* cache;

    static const uint32_t NUM_FEATURES = 8;
    static const uint32_t NUM_OUTPUTS = 3;

    void SetUp() override
    {
        consolidation_reset_global_state();
        brain = brain_create("test_cc_e2e", BRAIN_SIZE_SMALL,
                             BRAIN_TASK_CLASSIFICATION, NUM_FEATURES, NUM_OUTPUTS);
        ASSERT_NE(brain, nullptr);
        cache = consolidation_community_cache_create();
        ASSERT_NE(cache, nullptr);
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

    void train_batch(uint32_t count, uint32_t seed) {
        float features[NUM_FEATURES];
        srand(seed);
        for (uint32_t i = 0; i < count; i++) {
            for (uint32_t f = 0; f < NUM_FEATURES; f++) {
                features[f] = (float)(rand() % 1000) / 1000.0f;
            }
            const char* labels[] = {"class_0", "class_1", "class_2"};
            brain_learn_example(brain, features, NUM_FEATURES,
                                labels[i % NUM_OUTPUTS], 0.9f);
        }
    }

    bool can_predict(float* features) {
        brain_decision_t* decision = brain_decide(brain, features, NUM_FEATURES);
        bool ok = (decision != nullptr);
        if (decision) brain_free_decision(decision);
        return ok;
    }
};

//=============================================================================
// End-to-End Pipeline Tests
//=============================================================================

/**
 * WHAT: Test Athena-style multi-phase training pipeline
 * WHY:  Mimics the actual Athena training workflow:
 *       Phase 0 (warm-up) → light consolidation
 *       Phase 1 (core training) → cache communities → auto consolidation
 *       Phase 2 (hard examples) → cache refresh → auto consolidation
 *       Final → cache refresh → full consolidation
 */
TEST_F(CommunityCacheE2ETest, AthenaStyleMultiPhasePipeline)
{
    auto total_start = std::chrono::high_resolution_clock::now();

    // Phase 0: Warm-up
    train_batch(20, 42);
    {
        consolidation_config_t config = consolidation_light_config();
        bool result = brain_consolidate_memory(brain, &config);
        EXPECT_TRUE(result);
    }

    // Phase 1: Core training
    train_batch(50, 123);
    {
        bool cached = consolidation_cache_communities(cache, brain);
        EXPECT_TRUE(cached);
        EXPECT_TRUE(cache->is_valid);
        EXPECT_GT(cache->num_communities, 0u);

        consolidation_config_t config = consolidation_auto_config(cache->num_neurons);
        config.consolidation_cycles = 2;
        bool result = brain_consolidate_memory_community_aware(brain, &config, cache);
        EXPECT_TRUE(result);
    }

    // Phase 2: Hard examples
    train_batch(30, 456);
    {
        // Refresh cache
        consolidation_community_cache_invalidate(cache);
        bool recached = consolidation_cache_communities(cache, brain);
        EXPECT_TRUE(recached);

        consolidation_config_t config = consolidation_auto_config(cache->num_neurons);
        config.consolidation_cycles = 2;
        bool result = brain_consolidate_memory_community_aware(brain, &config, cache);
        EXPECT_TRUE(result);
    }

    // Final consolidation
    {
        consolidation_community_cache_invalidate(cache);
        bool recached = consolidation_cache_communities(cache, brain);
        EXPECT_TRUE(recached);

        consolidation_config_t config = consolidation_default_config();
        config.consolidation_cycles = 3;
        bool result = brain_consolidate_memory_community_aware(brain, &config, cache);
        EXPECT_TRUE(result);
    }

    auto total_end = std::chrono::high_resolution_clock::now();
    auto total_ms = std::chrono::duration_cast<std::chrono::milliseconds>(total_end - total_start).count();

    printf("Full multi-phase pipeline: %ld ms\n", total_ms);

    // Verify stats reflect multiple consolidations
    consolidation_stats_t stats;
    consolidation_get_stats(nullptr, &stats);
    EXPECT_GE(stats.total_consolidations, 3u);

    // Entire pipeline should complete in < 60 seconds
    EXPECT_LT(total_ms, 60000);
}

/**
 * WHAT: Test prediction works after community-aware consolidation
 * WHY:  Consolidation must not corrupt the brain's prediction capability
 */
TEST_F(CommunityCacheE2ETest, PredictionAfterConsolidation)
{
    train_batch(50, 42);

    consolidation_cache_communities(cache, brain);

    consolidation_config_t config = consolidation_default_config();
    config.consolidation_cycles = 3;
    brain_consolidate_memory_community_aware(brain, &config, cache);

    // Should still be able to predict without crashing
    float features[NUM_FEATURES] = {0.5f, 0.3f, 0.8f, 0.1f, 0.6f, 0.4f, 0.7f, 0.2f};
    bool ok = can_predict(features);
    EXPECT_TRUE(ok);
}

/**
 * WHAT: Test cache survives full brain lifecycle
 * WHY:  Verify no use-after-free or double-free in lifecycle
 */
TEST_F(CommunityCacheE2ETest, CacheFullLifecycle)
{
    // Create → populate → query → invalidate → repopulate → destroy
    train_batch(20, 1);

    bool pop1 = consolidation_cache_communities(cache, brain);
    EXPECT_TRUE(pop1);

    uint32_t comm = consolidation_cache_get_community(cache, 0);
    EXPECT_NE(comm, UINT32_MAX);

    consolidation_community_cache_invalidate(cache);
    EXPECT_FALSE(cache->is_valid);

    train_batch(20, 2);

    bool pop2 = consolidation_cache_communities(cache, brain);
    EXPECT_TRUE(pop2);
    EXPECT_TRUE(cache->is_valid);

    // Destroy happens in TearDown — no crash
}

/**
 * WHAT: Test interleaved standard and community-aware consolidation
 * WHY:  Real code may mix both paths depending on cache availability
 */
TEST_F(CommunityCacheE2ETest, InterleavedConsolidation)
{
    train_batch(30, 42);

    consolidation_config_t config = consolidation_default_config();
    config.consolidation_cycles = 1;

    // Standard (no cache)
    EXPECT_TRUE(brain_consolidate_memory(brain, &config));

    // Build cache
    consolidation_cache_communities(cache, brain);

    // Community-aware
    EXPECT_TRUE(brain_consolidate_memory_community_aware(brain, &config, cache));

    // Standard again
    EXPECT_TRUE(brain_consolidate_memory(brain, &config));

    // Community-aware again
    EXPECT_TRUE(brain_consolidate_memory_community_aware(brain, &config, cache));

    // Invalidate cache, then standard
    consolidation_community_cache_invalidate(cache);
    EXPECT_TRUE(brain_consolidate_memory(brain, &config));

    // Community-aware with invalid cache (should fall back)
    EXPECT_TRUE(brain_consolidate_memory_community_aware(brain, &config, cache));
}

/**
 * WHAT: Test community cache info is accessible
 * WHY:  Training scripts need to log community info for monitoring
 */
TEST_F(CommunityCacheE2ETest, CacheInfoAccessible)
{
    train_batch(30, 42);
    consolidation_cache_communities(cache, brain);

    ASSERT_TRUE(cache->is_valid);

    printf("Community cache info:\n");
    printf("  Neurons: %u\n", cache->num_neurons);
    printf("  Communities: %u\n", cache->num_communities);
    printf("  Modularity Q: %.3f\n", cache->modularity);
    printf("  Hubs: %u\n", cache->num_hubs);
    printf("  Timestamp: %lu ms\n", (unsigned long)cache->cache_timestamp_ms);

    EXPECT_GT(cache->num_neurons, 0u);
    EXPECT_GT(cache->num_communities, 0u);
    EXPECT_GE(cache->modularity, -0.5f);
    EXPECT_LE(cache->modularity, 1.0f);
}

/**
 * WHAT: Test pipeline performance: cache + consolidation within time budget
 * WHY:  The combined operation must not exceed reasonable time bounds
 */
TEST_F(CommunityCacheE2ETest, PipelinePerformanceBudget)
{
    train_batch(50, 42);

    auto start = std::chrono::high_resolution_clock::now();

    // Cache communities
    consolidation_cache_communities(cache, brain);

    // Consolidate with cache
    consolidation_config_t config = consolidation_auto_config(cache->num_neurons);
    config.consolidation_cycles = 3;
    brain_consolidate_memory_community_aware(brain, &config, cache);

    auto end = std::chrono::high_resolution_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    printf("Cache + consolidation: %ld ms\n", ms);

    // Should complete in < 30 seconds for BRAIN_SIZE_SMALL
    EXPECT_LT(ms, 30000);
}
