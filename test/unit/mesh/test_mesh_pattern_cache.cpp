/**
 * @file test_mesh_pattern_cache.cpp
 * @brief Unit tests for Pattern Cache with CoW semantics
 *
 * Tests pattern activation caching:
 * - Cache creation and configuration
 * - Hash computation and collision handling
 * - Lookup and store operations
 * - Copy-on-Write (CoW) via nimcp_cache integration
 * - LRU eviction and TTL expiration
 * - Module-based invalidation
 *
 * @author NIMCP Development Team
 * @date 2025-02-01
 */

#include <gtest/gtest.h>
#include <cmath>

extern "C" {
#include "mesh/nimcp_mesh_pattern_cache.h"
#include "mesh/nimcp_mesh_pattern_routing.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/cache/nimcp_cache.h"
}

class MeshPatternCacheTest : public ::testing::Test {
protected:
    pattern_cache_t* cache = nullptr;

    void SetUp() override {
        /* Initialize nimcp_cache system */
        nimcp_cache_init();

        pattern_cache_config_t config;
        pattern_cache_default_config(&config);
        config.max_entries = 256;
        config.default_ttl_ms = 1000;
        config.enable_cow = true;
        config.enable_lru = true;
        config.enable_logging = false;  /* Quiet for tests */

        cache = pattern_cache_create(&config);
    }

    void TearDown() override {
        if (cache) {
            pattern_cache_destroy(cache);
            cache = nullptr;
        }
    }

    /* Helper to create a test pattern */
    mesh_pattern_t make_pattern(float base_value, uint32_t active_dims = 8) {
        mesh_pattern_t p;
        memset(&p, 0, sizeof(p));

        for (uint32_t i = 0; i < active_dims && i < MESH_PATTERN_DIM; i++) {
            p.vector[i] = base_value + (float)i * 0.1f;
        }

        float sum = 0.0f;
        for (int i = 0; i < MESH_PATTERN_DIM; i++) {
            sum += p.vector[i] * p.vector[i];
        }
        p.magnitude = sqrtf(sum);
        p.active_dims = active_dims;

        return p;
    }

    /* Helper to create test activations */
    void make_activations(cached_activation_t* activations, size_t count) {
        for (size_t i = 0; i < count; i++) {
            activations[i].module_id = (mesh_participant_id_t)(0x100 + i);
            activations[i].activation_level = 0.5f + (float)i * 0.1f;
            activations[i].similarity = 0.8f;
            activations[i].role = ENDORSER_ROLE_PREFERRED;
            activations[i].should_endorse = true;
        }
    }
};

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

TEST_F(MeshPatternCacheTest, CreateCache) {
    ASSERT_NE(cache, nullptr);
}

TEST_F(MeshPatternCacheTest, CreateCacheWithDefaults) {
    pattern_cache_t* c = pattern_cache_create(NULL);
    ASSERT_NE(c, nullptr);
    pattern_cache_destroy(c);
}

TEST_F(MeshPatternCacheTest, CreateCacheWithConfig) {
    pattern_cache_config_t config;
    ASSERT_EQ(pattern_cache_default_config(&config), NIMCP_SUCCESS);

    config.max_entries = 128;
    config.default_ttl_ms = 5000;
    config.enable_cow = false;

    pattern_cache_t* c = pattern_cache_create(&config);
    ASSERT_NE(c, nullptr);
    pattern_cache_destroy(c);
}

TEST_F(MeshPatternCacheTest, DefaultConfig) {
    pattern_cache_config_t config;
    ASSERT_EQ(pattern_cache_default_config(&config), NIMCP_SUCCESS);

    EXPECT_EQ(config.max_entries, PATTERN_CACHE_MAX_ENTRIES);
    EXPECT_EQ(config.default_ttl_ms, PATTERN_CACHE_DEFAULT_TTL_MS);
    EXPECT_TRUE(config.enable_cow);
    EXPECT_TRUE(config.enable_lru);
}

TEST_F(MeshPatternCacheTest, DefaultConfigNull) {
    EXPECT_EQ(pattern_cache_default_config(NULL), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(MeshPatternCacheTest, DestroyCacheNull) {
    pattern_cache_destroy(NULL);  /* Should not crash */
}

TEST_F(MeshPatternCacheTest, ClearCache) {
    mesh_pattern_t pattern = make_pattern(1.0f);
    cached_activation_t activations[2];
    make_activations(activations, 2);

    ASSERT_EQ(pattern_cache_store(cache, &pattern, activations, 2, 0), NIMCP_SUCCESS);

    ASSERT_EQ(pattern_cache_clear(cache), NIMCP_SUCCESS);

    /* Lookup should miss after clear */
    cached_activation_t out[2];
    size_t count = 0;
    EXPECT_EQ(pattern_cache_lookup(cache, &pattern, out, 2, &count),
              NIMCP_ERROR_NOT_FOUND);
}

TEST_F(MeshPatternCacheTest, ClearCacheNull) {
    EXPECT_EQ(pattern_cache_clear(NULL), NIMCP_ERROR_INVALID_PARAM);
}

/* ============================================================================
 * Hash Tests
 * ============================================================================ */

TEST_F(MeshPatternCacheTest, ComputeHash) {
    mesh_pattern_t pattern = make_pattern(1.0f);
    pattern_hash_t hash;

    ASSERT_EQ(pattern_cache_hash(&pattern, &hash), NIMCP_SUCCESS);

    /* Hash should be non-zero */
    bool all_zero = true;
    for (int i = 0; i < PATTERN_HASH_SIZE; i++) {
        if (hash.bytes[i] != 0) {
            all_zero = false;
            break;
        }
    }
    EXPECT_FALSE(all_zero);
}

TEST_F(MeshPatternCacheTest, ComputeHashNull) {
    mesh_pattern_t pattern = make_pattern(1.0f);
    pattern_hash_t hash;

    EXPECT_EQ(pattern_cache_hash(NULL, &hash), NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(pattern_cache_hash(&pattern, NULL), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(MeshPatternCacheTest, HashDeterministic) {
    mesh_pattern_t pattern = make_pattern(1.0f);
    pattern_hash_t h1, h2;

    ASSERT_EQ(pattern_cache_hash(&pattern, &h1), NIMCP_SUCCESS);
    ASSERT_EQ(pattern_cache_hash(&pattern, &h2), NIMCP_SUCCESS);

    EXPECT_TRUE(pattern_hash_equals(&h1, &h2));
}

TEST_F(MeshPatternCacheTest, HashDifferentPatterns) {
    mesh_pattern_t p1 = make_pattern(1.0f);
    mesh_pattern_t p2 = make_pattern(2.0f);

    pattern_hash_t h1, h2;
    ASSERT_EQ(pattern_cache_hash(&p1, &h1), NIMCP_SUCCESS);
    ASSERT_EQ(pattern_cache_hash(&p2, &h2), NIMCP_SUCCESS);

    EXPECT_FALSE(pattern_hash_equals(&h1, &h2));
}

TEST_F(MeshPatternCacheTest, HashEquals) {
    pattern_hash_t a, b;
    memset(&a, 0x12, sizeof(a));
    memset(&b, 0x12, sizeof(b));

    EXPECT_TRUE(pattern_hash_equals(&a, &b));

    b.bytes[0] = 0x00;
    EXPECT_FALSE(pattern_hash_equals(&a, &b));
}

TEST_F(MeshPatternCacheTest, HashEqualsNull) {
    pattern_hash_t hash;
    EXPECT_FALSE(pattern_hash_equals(NULL, &hash));
    EXPECT_FALSE(pattern_hash_equals(&hash, NULL));
}

TEST_F(MeshPatternCacheTest, HashToString) {
    pattern_hash_t hash;
    memset(hash.bytes, 0xAB, sizeof(hash.bytes));

    char buf[64];
    pattern_hash_to_string(&hash, buf, sizeof(buf));

    EXPECT_STREQ(buf, "abababababababababababababababab");
}

TEST_F(MeshPatternCacheTest, HashToStringNull) {
    char buf[64];
    pattern_hash_t hash;

    pattern_hash_to_string(NULL, buf, sizeof(buf));
    EXPECT_STREQ(buf, "");

    pattern_hash_to_string(&hash, NULL, sizeof(buf));  /* Should not crash */

    buf[0] = 'x';
    pattern_hash_to_string(&hash, buf, 0);
    EXPECT_EQ(buf[0], 'x');  /* Unchanged when buf_size is 0 */
}

/* ============================================================================
 * Store and Lookup Tests
 * ============================================================================ */

TEST_F(MeshPatternCacheTest, StoreAndLookup) {
    mesh_pattern_t pattern = make_pattern(1.0f);
    cached_activation_t activations[3];
    make_activations(activations, 3);

    ASSERT_EQ(pattern_cache_store(cache, &pattern, activations, 3, 0), NIMCP_SUCCESS);

    cached_activation_t out[8];
    size_t count = 0;
    ASSERT_EQ(pattern_cache_lookup(cache, &pattern, out, 8, &count), NIMCP_SUCCESS);

    EXPECT_EQ(count, 3u);
    EXPECT_EQ(out[0].module_id, 0x100ull);
    EXPECT_EQ(out[1].module_id, 0x101ull);
    EXPECT_EQ(out[2].module_id, 0x102ull);
}

TEST_F(MeshPatternCacheTest, StoreNull) {
    mesh_pattern_t pattern = make_pattern(1.0f);
    cached_activation_t activations[1];
    make_activations(activations, 1);

    EXPECT_EQ(pattern_cache_store(NULL, &pattern, activations, 1, 0),
              NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(pattern_cache_store(cache, NULL, activations, 1, 0),
              NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(pattern_cache_store(cache, &pattern, NULL, 1, 0),
              NIMCP_ERROR_NULL_POINTER);
}

TEST_F(MeshPatternCacheTest, LookupNull) {
    mesh_pattern_t pattern = make_pattern(1.0f);
    cached_activation_t out[8];
    size_t count = 0;

    EXPECT_EQ(pattern_cache_lookup(NULL, &pattern, out, 8, &count),
              NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(pattern_cache_lookup(cache, NULL, out, 8, &count),
              NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(pattern_cache_lookup(cache, &pattern, NULL, 8, &count),
              NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(pattern_cache_lookup(cache, &pattern, out, 8, NULL),
              NIMCP_ERROR_NULL_POINTER);
}

TEST_F(MeshPatternCacheTest, LookupMiss) {
    mesh_pattern_t pattern = make_pattern(1.0f);

    cached_activation_t out[8];
    size_t count = 0;
    EXPECT_EQ(pattern_cache_lookup(cache, &pattern, out, 8, &count),
              NIMCP_ERROR_NOT_FOUND);
    EXPECT_EQ(count, 0u);
}

TEST_F(MeshPatternCacheTest, StoreUpdate) {
    mesh_pattern_t pattern = make_pattern(1.0f);

    cached_activation_t act1[1];
    make_activations(act1, 1);
    act1[0].activation_level = 0.5f;

    ASSERT_EQ(pattern_cache_store(cache, &pattern, act1, 1, 0), NIMCP_SUCCESS);

    /* Update with new value */
    cached_activation_t act2[1];
    make_activations(act2, 1);
    act2[0].activation_level = 0.9f;

    ASSERT_EQ(pattern_cache_store(cache, &pattern, act2, 1, 0), NIMCP_SUCCESS);

    /* Lookup should return updated value */
    cached_activation_t out[1];
    size_t count = 0;
    ASSERT_EQ(pattern_cache_lookup(cache, &pattern, out, 1, &count), NIMCP_SUCCESS);
    EXPECT_EQ(count, 1u);
    EXPECT_FLOAT_EQ(out[0].activation_level, 0.9f);
}

TEST_F(MeshPatternCacheTest, StoreMultiplePatterns) {
    for (int i = 0; i < 10; i++) {
        mesh_pattern_t pattern = make_pattern((float)i);
        cached_activation_t activations[1];
        make_activations(activations, 1);
        activations[0].module_id = (mesh_participant_id_t)(0x1000 + i);

        ASSERT_EQ(pattern_cache_store(cache, &pattern, activations, 1, 0), NIMCP_SUCCESS);
    }

    /* Verify all can be looked up */
    for (int i = 0; i < 10; i++) {
        mesh_pattern_t pattern = make_pattern((float)i);
        cached_activation_t out[1];
        size_t count = 0;

        ASSERT_EQ(pattern_cache_lookup(cache, &pattern, out, 1, &count), NIMCP_SUCCESS);
        EXPECT_EQ(count, 1u);
        EXPECT_EQ(out[0].module_id, (mesh_participant_id_t)(0x1000 + i));
    }
}

/* ============================================================================
 * Invalidation Tests
 * ============================================================================ */

TEST_F(MeshPatternCacheTest, InvalidatePattern) {
    mesh_pattern_t pattern = make_pattern(1.0f);
    cached_activation_t activations[1];
    make_activations(activations, 1);

    ASSERT_EQ(pattern_cache_store(cache, &pattern, activations, 1, 0), NIMCP_SUCCESS);

    ASSERT_EQ(pattern_cache_invalidate(cache, &pattern), NIMCP_SUCCESS);

    /* Lookup should miss after invalidation */
    cached_activation_t out[1];
    size_t count = 0;
    EXPECT_EQ(pattern_cache_lookup(cache, &pattern, out, 1, &count),
              NIMCP_ERROR_NOT_FOUND);
}

TEST_F(MeshPatternCacheTest, InvalidatePatternNull) {
    mesh_pattern_t pattern = make_pattern(1.0f);

    EXPECT_EQ(pattern_cache_invalidate(NULL, &pattern), NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(pattern_cache_invalidate(cache, NULL), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(MeshPatternCacheTest, InvalidateModule) {
    /* Store patterns with different modules */
    for (int i = 0; i < 5; i++) {
        mesh_pattern_t pattern = make_pattern((float)i);
        cached_activation_t activations[1];
        make_activations(activations, 1);
        activations[0].module_id = 0x200;  /* Same module for all */

        ASSERT_EQ(pattern_cache_store(cache, &pattern, activations, 1, 0), NIMCP_SUCCESS);
    }

    /* Store one with different module */
    mesh_pattern_t other = make_pattern(100.0f);
    cached_activation_t other_act[1];
    make_activations(other_act, 1);
    other_act[0].module_id = 0x300;  /* Different module */
    ASSERT_EQ(pattern_cache_store(cache, &other, other_act, 1, 0), NIMCP_SUCCESS);

    /* Invalidate module 0x200 */
    ASSERT_EQ(pattern_cache_invalidate_module(cache, 0x200), NIMCP_SUCCESS);

    /* All patterns with 0x200 should miss */
    for (int i = 0; i < 5; i++) {
        mesh_pattern_t pattern = make_pattern((float)i);
        cached_activation_t out[1];
        size_t count = 0;
        EXPECT_EQ(pattern_cache_lookup(cache, &pattern, out, 1, &count),
                  NIMCP_ERROR_NOT_FOUND);
    }

    /* Other pattern should still be found */
    cached_activation_t out[1];
    size_t count = 0;
    EXPECT_EQ(pattern_cache_lookup(cache, &other, out, 1, &count), NIMCP_SUCCESS);
    EXPECT_EQ(count, 1u);
}

TEST_F(MeshPatternCacheTest, InvalidateModuleNull) {
    EXPECT_EQ(pattern_cache_invalidate_module(NULL, 0x100), NIMCP_ERROR_NULL_POINTER);
}

/* ============================================================================
 * Copy-on-Write Tests
 * ============================================================================ */

TEST_F(MeshPatternCacheTest, AcquireRelease) {
    mesh_pattern_t pattern = make_pattern(1.0f);
    cached_activation_t activations[1];
    make_activations(activations, 1);

    ASSERT_EQ(pattern_cache_store(cache, &pattern, activations, 1, 0), NIMCP_SUCCESS);

    const pattern_cache_entry_t* entry = pattern_cache_acquire(cache, &pattern);
    ASSERT_NE(entry, nullptr);

    EXPECT_EQ(entry->activation_count, 1u);
    EXPECT_EQ(entry->activations[0].module_id, 0x100ull);

    pattern_cache_release(cache, entry);
}

TEST_F(MeshPatternCacheTest, AcquireNull) {
    mesh_pattern_t pattern = make_pattern(1.0f);

    EXPECT_EQ(pattern_cache_acquire(NULL, &pattern), nullptr);
    EXPECT_EQ(pattern_cache_acquire(cache, NULL), nullptr);
}

TEST_F(MeshPatternCacheTest, AcquireMiss) {
    mesh_pattern_t pattern = make_pattern(1.0f);
    EXPECT_EQ(pattern_cache_acquire(cache, &pattern), nullptr);
}

TEST_F(MeshPatternCacheTest, ReleaseNull) {
    pattern_cache_release(NULL, nullptr);
    pattern_cache_release(cache, NULL);
    /* Should not crash */
}

TEST_F(MeshPatternCacheTest, CoWCopy) {
    mesh_pattern_t original = make_pattern(1.0f);
    cached_activation_t activations[1];
    make_activations(activations, 1);

    ASSERT_EQ(pattern_cache_store(cache, &original, activations, 1, 0), NIMCP_SUCCESS);

    mesh_pattern_t variant = make_pattern(1.1f);  /* Slightly different */
    pattern_cache_entry_t* new_entry = nullptr;

    ASSERT_EQ(pattern_cache_cow_copy(cache, &original, &variant, &new_entry), NIMCP_SUCCESS);
    ASSERT_NE(new_entry, nullptr);

    /* New entry should have variant pattern */
    EXPECT_FLOAT_EQ(new_entry->pattern.vector[0], 1.1f);

    /* Original should still be valid */
    cached_activation_t out[1];
    size_t count = 0;
    EXPECT_EQ(pattern_cache_lookup(cache, &original, out, 1, &count), NIMCP_SUCCESS);
}

TEST_F(MeshPatternCacheTest, CoWCopyNull) {
    mesh_pattern_t p1 = make_pattern(1.0f);
    mesh_pattern_t p2 = make_pattern(2.0f);
    pattern_cache_entry_t* entry = nullptr;

    EXPECT_EQ(pattern_cache_cow_copy(NULL, &p1, &p2, &entry), NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(pattern_cache_cow_copy(cache, NULL, &p2, &entry), NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(pattern_cache_cow_copy(cache, &p1, NULL, &entry), NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(pattern_cache_cow_copy(cache, &p1, &p2, NULL), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(MeshPatternCacheTest, CoWCopyNotFound) {
    mesh_pattern_t p1 = make_pattern(1.0f);  /* Not stored */
    mesh_pattern_t p2 = make_pattern(2.0f);
    pattern_cache_entry_t* entry = nullptr;

    EXPECT_EQ(pattern_cache_cow_copy(cache, &p1, &p2, &entry), NIMCP_ERROR_NOT_FOUND);
}

/* ============================================================================
 * Eviction Tests
 * ============================================================================ */

TEST_F(MeshPatternCacheTest, EvictExpired) {
    mesh_pattern_t pattern = make_pattern(1.0f);
    cached_activation_t activations[1];
    make_activations(activations, 1);

    /* Store with very short TTL */
    ASSERT_EQ(pattern_cache_store(cache, &pattern, activations, 1, 1), NIMCP_SUCCESS);

    /* Wait for expiration */
    usleep(5000);  /* 5ms */

    ASSERT_EQ(pattern_cache_evict_expired(cache), NIMCP_SUCCESS);

    /* Should be evicted */
    cached_activation_t out[1];
    size_t count = 0;
    EXPECT_EQ(pattern_cache_lookup(cache, &pattern, out, 1, &count),
              NIMCP_ERROR_NOT_FOUND);
}

TEST_F(MeshPatternCacheTest, EvictExpiredNull) {
    EXPECT_EQ(pattern_cache_evict_expired(NULL), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(MeshPatternCacheTest, EvictLRU) {
    /* Fill cache with patterns */
    for (int i = 0; i < 50; i++) {
        mesh_pattern_t pattern = make_pattern((float)i);
        cached_activation_t activations[1];
        make_activations(activations, 1);

        ASSERT_EQ(pattern_cache_store(cache, &pattern, activations, 1, 0), NIMCP_SUCCESS);
    }

    /* Access first pattern to make it recently used */
    mesh_pattern_t first = make_pattern(0.0f);
    cached_activation_t out[1];
    size_t count = 0;
    ASSERT_EQ(pattern_cache_lookup(cache, &first, out, 1, &count), NIMCP_SUCCESS);

    /* Evict some entries */
    ASSERT_EQ(pattern_cache_evict_lru(cache, 5), NIMCP_SUCCESS);

    /* First should still be present (recently accessed) */
    EXPECT_EQ(pattern_cache_lookup(cache, &first, out, 1, &count), NIMCP_SUCCESS);
}

TEST_F(MeshPatternCacheTest, EvictLRUNull) {
    EXPECT_EQ(pattern_cache_evict_lru(NULL, 1), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(MeshPatternCacheTest, EvictLRUZero) {
    EXPECT_EQ(pattern_cache_evict_lru(cache, 0), NIMCP_SUCCESS);
}

/* ============================================================================
 * Statistics Tests
 * ============================================================================ */

TEST_F(MeshPatternCacheTest, GetStats) {
    mesh_pattern_t pattern = make_pattern(1.0f);
    cached_activation_t activations[1];
    make_activations(activations, 1);

    ASSERT_EQ(pattern_cache_store(cache, &pattern, activations, 1, 0), NIMCP_SUCCESS);

    /* Hit */
    cached_activation_t out[1];
    size_t count = 0;
    ASSERT_EQ(pattern_cache_lookup(cache, &pattern, out, 1, &count), NIMCP_SUCCESS);

    /* Miss */
    mesh_pattern_t missing = make_pattern(999.0f);
    pattern_cache_lookup(cache, &missing, out, 1, &count);

    pattern_cache_stats_t stats;
    ASSERT_EQ(pattern_cache_get_stats(cache, &stats), NIMCP_SUCCESS);

    EXPECT_GE(stats.hits, 1u);
    EXPECT_GE(stats.misses, 1u);
    EXPECT_EQ(stats.current_entries, 1u);
    EXPECT_GT(stats.hit_rate, 0.0f);
}

TEST_F(MeshPatternCacheTest, GetStatsNull) {
    pattern_cache_stats_t stats;
    EXPECT_EQ(pattern_cache_get_stats(NULL, &stats), NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(pattern_cache_get_stats(cache, NULL), NIMCP_ERROR_NULL_POINTER);
}

/* ============================================================================
 * Print Tests (for coverage)
 * ============================================================================ */

TEST_F(MeshPatternCacheTest, PrintNull) {
    pattern_cache_print(NULL);  /* Should not crash */
}

TEST_F(MeshPatternCacheTest, Print) {
    mesh_pattern_t pattern = make_pattern(1.0f);
    cached_activation_t activations[1];
    make_activations(activations, 1);

    pattern_cache_store(cache, &pattern, activations, 1, 0);

    /* Hit */
    cached_activation_t out[1];
    size_t count = 0;
    pattern_cache_lookup(cache, &pattern, out, 1, &count);

    pattern_cache_print(cache);  /* Should not crash */
}
