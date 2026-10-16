/**
 * @file test_hopfield_memory.cpp
 * @brief Unit tests for Modern Hopfield Memory
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

/* Headers have their own extern "C" guards - don't wrap them to avoid
 * CUDA C++ function conflicts */
#include "cognitive/memory/nimcp_hopfield_memory.h"

class HopfieldMemoryTest : public ::testing::Test {
protected:
    hopfield_memory_t* memory = nullptr;
    hopfield_config_t config;

    void SetUp() override {
        hopfield_default_config(&config);
        config.pattern_dim = 64;
        config.capacity = 100;
        config.gpu_mode = HOPFIELD_GPU_DISABLED;
    }

    void TearDown() override {
        if (memory) {
            hopfield_memory_destroy(memory);
            memory = nullptr;
        }
    }

    void FillPattern(float* pattern, float value) {
        for (uint32_t i = 0; i < config.pattern_dim; i++) {
            pattern[i] = value + (float)i / config.pattern_dim;
        }
    }
};

/* ============================================================================
 * Configuration Tests
 * ============================================================================ */

TEST_F(HopfieldMemoryTest, DefaultConfigValid) {
    hopfield_config_t cfg;
    ASSERT_EQ(hopfield_default_config(&cfg), NIMCP_SUCCESS);
    EXPECT_EQ(cfg.pattern_dim, HOPFIELD_DEFAULT_DIM);
    EXPECT_EQ(cfg.capacity, HOPFIELD_DEFAULT_CAPACITY);
    EXPECT_EQ(cfg.mode, HOPFIELD_MODE_SOFTMAX);
    EXPECT_GT(cfg.beta, 0.0f);
}

TEST_F(HopfieldMemoryTest, ValidateConfigNullReturnsError) {
    EXPECT_NE(hopfield_validate_config(nullptr), NIMCP_SUCCESS);
}

TEST_F(HopfieldMemoryTest, ValidateConfigZeroDimReturnsError) {
    config.pattern_dim = 0;
    EXPECT_NE(hopfield_validate_config(&config), NIMCP_SUCCESS);
}

TEST_F(HopfieldMemoryTest, ValidateConfigZeroCapacityReturnsError) {
    config.capacity = 0;
    EXPECT_NE(hopfield_validate_config(&config), NIMCP_SUCCESS);
}

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

TEST_F(HopfieldMemoryTest, CreateWithDefaultConfig) {
    memory = hopfield_memory_create(nullptr);
    ASSERT_NE(memory, nullptr);
}

TEST_F(HopfieldMemoryTest, CreateWithCustomConfig) {
    memory = hopfield_memory_create(&config);
    ASSERT_NE(memory, nullptr);
}

TEST_F(HopfieldMemoryTest, DestroyNullIsSafe) {
    hopfield_memory_destroy(nullptr);
    SUCCEED();
}

TEST_F(HopfieldMemoryTest, ClearSucceeds) {
    memory = hopfield_memory_create(&config);
    ASSERT_NE(memory, nullptr);

    EXPECT_EQ(hopfield_memory_clear(memory), NIMCP_SUCCESS);
    EXPECT_EQ(hopfield_memory_pattern_count(memory), 0);
}

/* ============================================================================
 * Storage Tests
 * ============================================================================ */

TEST_F(HopfieldMemoryTest, StorePatternSucceeds) {
    memory = hopfield_memory_create(&config);
    ASSERT_NE(memory, nullptr);

    float pattern[64];
    FillPattern(pattern, 1.0f);

    uint32_t id = 0;
    EXPECT_EQ(hopfield_memory_store(memory, pattern, &id), NIMCP_SUCCESS);
    EXPECT_GT(id, 0);
    EXPECT_EQ(hopfield_memory_pattern_count(memory), 1);
}

TEST_F(HopfieldMemoryTest, StoreMultiplePatternsSucceeds) {
    memory = hopfield_memory_create(&config);
    ASSERT_NE(memory, nullptr);

    float pattern[64];

    for (int i = 0; i < 10; i++) {
        FillPattern(pattern, (float)i);
        EXPECT_EQ(hopfield_memory_store(memory, pattern, nullptr), NIMCP_SUCCESS);
    }

    EXPECT_EQ(hopfield_memory_pattern_count(memory), 10);
}

TEST_F(HopfieldMemoryTest, StoreWithMetadataSucceeds) {
    memory = hopfield_memory_create(&config);
    ASSERT_NE(memory, nullptr);

    float pattern[64];
    FillPattern(pattern, 1.0f);

    uint32_t id = 0;
    EXPECT_EQ(hopfield_memory_store_with_meta(memory, pattern, 0.8f, nullptr, &id),
              NIMCP_SUCCESS);
    EXPECT_GT(id, 0);
}

TEST_F(HopfieldMemoryTest, StoreBatchSucceeds) {
    memory = hopfield_memory_create(&config);
    ASSERT_NE(memory, nullptr);

    float patterns[5 * 64];
    for (int i = 0; i < 5; i++) {
        for (uint32_t j = 0; j < 64; j++) {
            patterns[i * 64 + j] = (float)i + (float)j / 64;
        }
    }

    EXPECT_EQ(hopfield_memory_store_batch(memory, patterns, 5, nullptr), NIMCP_SUCCESS);
    EXPECT_EQ(hopfield_memory_pattern_count(memory), 5);
}

TEST_F(HopfieldMemoryTest, UpdatePatternSucceeds) {
    memory = hopfield_memory_create(&config);
    ASSERT_NE(memory, nullptr);

    float pattern[64];
    FillPattern(pattern, 1.0f);

    uint32_t id = 0;
    hopfield_memory_store(memory, pattern, &id);

    FillPattern(pattern, 2.0f);
    EXPECT_EQ(hopfield_memory_update_pattern(memory, id, pattern), NIMCP_SUCCESS);
}

TEST_F(HopfieldMemoryTest, RemovePatternSucceeds) {
    memory = hopfield_memory_create(&config);
    ASSERT_NE(memory, nullptr);

    float pattern[64];
    FillPattern(pattern, 1.0f);

    uint32_t id = 0;
    hopfield_memory_store(memory, pattern, &id);
    EXPECT_EQ(hopfield_memory_pattern_count(memory), 1);

    EXPECT_EQ(hopfield_memory_remove_pattern(memory, id), NIMCP_SUCCESS);
    EXPECT_EQ(hopfield_memory_pattern_count(memory), 0);
}

/* ============================================================================
 * Retrieval Tests
 * ============================================================================ */

TEST_F(HopfieldMemoryTest, RetrieveEmptyMemoryFails) {
    memory = hopfield_memory_create(&config);
    ASSERT_NE(memory, nullptr);

    float query[64];
    FillPattern(query, 1.0f);

    hopfield_retrieval_result_t* result = hopfield_result_create(config.pattern_dim);
    ASSERT_NE(result, nullptr);

    EXPECT_NE(hopfield_memory_retrieve(memory, query, result), NIMCP_SUCCESS);

    hopfield_result_destroy(result);
}

TEST_F(HopfieldMemoryTest, RetrieveSucceeds) {
    memory = hopfield_memory_create(&config);
    ASSERT_NE(memory, nullptr);

    float pattern[64];
    FillPattern(pattern, 1.0f);
    hopfield_memory_store(memory, pattern, nullptr);

    float query[64];
    FillPattern(query, 1.0f);

    hopfield_retrieval_result_t* result = hopfield_result_create(config.pattern_dim);
    ASSERT_NE(result, nullptr);

    EXPECT_EQ(hopfield_memory_retrieve(memory, query, result), NIMCP_SUCCESS);
    EXPECT_GT(result->similarity, 0.9f);

    hopfield_result_destroy(result);
}

TEST_F(HopfieldMemoryTest, RetrieveFindsBestMatch) {
    memory = hopfield_memory_create(&config);
    ASSERT_NE(memory, nullptr);

    float pattern1[64], pattern2[64], pattern3[64];
    FillPattern(pattern1, 0.0f);
    FillPattern(pattern2, 1.0f);
    FillPattern(pattern3, 2.0f);

    hopfield_memory_store(memory, pattern1, nullptr);
    hopfield_memory_store(memory, pattern2, nullptr);
    hopfield_memory_store(memory, pattern3, nullptr);

    float query[64];
    FillPattern(query, 1.0f);

    hopfield_retrieval_result_t* result = hopfield_result_create(config.pattern_dim);
    ASSERT_NE(result, nullptr);

    EXPECT_EQ(hopfield_memory_retrieve(memory, query, result), NIMCP_SUCCESS);
    EXPECT_GT(result->similarity, 0.9f);

    hopfield_result_destroy(result);
}

TEST_F(HopfieldMemoryTest, RetrieveWithIterationsSucceeds) {
    memory = hopfield_memory_create(&config);
    ASSERT_NE(memory, nullptr);

    float pattern[64];
    FillPattern(pattern, 1.0f);
    hopfield_memory_store(memory, pattern, nullptr);

    float query[64];
    FillPattern(query, 1.1f);

    hopfield_retrieval_result_t* result = hopfield_result_create(config.pattern_dim);
    ASSERT_NE(result, nullptr);

    EXPECT_EQ(hopfield_memory_retrieve_iter(memory, query, 5, result), NIMCP_SUCCESS);
    EXPECT_GE(result->iterations, 1);
    EXPECT_LE(result->iterations, 5);

    hopfield_result_destroy(result);
}

TEST_F(HopfieldMemoryTest, TopKSucceeds) {
    memory = hopfield_memory_create(&config);
    ASSERT_NE(memory, nullptr);

    float pattern[64];
    for (int i = 0; i < 5; i++) {
        FillPattern(pattern, (float)i);
        hopfield_memory_store(memory, pattern, nullptr);
    }

    float query[64];
    FillPattern(query, 2.5f);

    uint32_t ids[3];
    float sims[3];

    EXPECT_EQ(hopfield_memory_top_k(memory, query, 3, ids, sims), NIMCP_SUCCESS);

    EXPECT_GT(sims[0], sims[1]);
    EXPECT_GT(sims[1], sims[2]);
}

/* ============================================================================
 * Energy Tests
 * ============================================================================ */

TEST_F(HopfieldMemoryTest, ComputeEnergyReturnsValue) {
    memory = hopfield_memory_create(&config);
    ASSERT_NE(memory, nullptr);

    float pattern[64];
    FillPattern(pattern, 1.0f);
    hopfield_memory_store(memory, pattern, nullptr);

    float energy = hopfield_memory_compute_energy(memory, pattern);
    EXPECT_FALSE(std::isnan(energy));
}

TEST_F(HopfieldMemoryTest, ComputeEnergyEmptyReturnsNaN) {
    memory = hopfield_memory_create(&config);
    ASSERT_NE(memory, nullptr);

    float pattern[64];
    FillPattern(pattern, 1.0f);

    float energy = hopfield_memory_compute_energy(memory, pattern);
    EXPECT_TRUE(std::isnan(energy));
}

TEST_F(HopfieldMemoryTest, GetSimilaritiesSucceeds) {
    memory = hopfield_memory_create(&config);
    ASSERT_NE(memory, nullptr);

    float pattern[64];
    for (int i = 0; i < 5; i++) {
        FillPattern(pattern, (float)i);
        hopfield_memory_store(memory, pattern, nullptr);
    }

    float query[64];
    FillPattern(query, 2.0f);

    float sims[5];
    EXPECT_EQ(hopfield_memory_get_similarities(memory, query, sims), NIMCP_SUCCESS);

    for (int i = 0; i < 5; i++) {
        EXPECT_FALSE(std::isnan(sims[i]));
    }
}

/* ============================================================================
 * Statistics Tests
 * ============================================================================ */

TEST_F(HopfieldMemoryTest, GetStatsSucceeds) {
    memory = hopfield_memory_create(&config);
    ASSERT_NE(memory, nullptr);

    hopfield_stats_t stats;
    EXPECT_EQ(hopfield_memory_get_stats(memory, &stats), NIMCP_SUCCESS);
    EXPECT_EQ(stats.patterns_stored, 0);
}

TEST_F(HopfieldMemoryTest, StatsUpdateAfterStore) {
    memory = hopfield_memory_create(&config);
    ASSERT_NE(memory, nullptr);

    float pattern[64];
    FillPattern(pattern, 1.0f);
    hopfield_memory_store(memory, pattern, nullptr);

    hopfield_stats_t stats;
    hopfield_memory_get_stats(memory, &stats);
    EXPECT_EQ(stats.patterns_stored, 1);
    EXPECT_EQ(stats.total_stores, 1);
}

TEST_F(HopfieldMemoryTest, ResetStatsSucceeds) {
    memory = hopfield_memory_create(&config);
    ASSERT_NE(memory, nullptr);

    EXPECT_EQ(hopfield_memory_reset_stats(memory), NIMCP_SUCCESS);
}

TEST_F(HopfieldMemoryTest, PatternCountAndCapacity) {
    memory = hopfield_memory_create(&config);
    ASSERT_NE(memory, nullptr);

    EXPECT_EQ(hopfield_memory_pattern_count(memory), 0);
    EXPECT_EQ(hopfield_memory_capacity(memory), config.capacity);

    float pattern[64];
    FillPattern(pattern, 1.0f);
    hopfield_memory_store(memory, pattern, nullptr);

    EXPECT_EQ(hopfield_memory_pattern_count(memory), 1);
}

/* ============================================================================
 * String Conversion Tests
 * ============================================================================ */

TEST_F(HopfieldMemoryTest, ModeToString) {
    EXPECT_STREQ(hopfield_mode_to_string(HOPFIELD_MODE_SOFTMAX), "SOFTMAX");
    EXPECT_STREQ(hopfield_mode_to_string(HOPFIELD_MODE_EXPONENTIAL), "EXPONENTIAL");
    EXPECT_STREQ(hopfield_mode_to_string(HOPFIELD_MODE_POLYNOMIAL), "POLYNOMIAL");
    EXPECT_STREQ(hopfield_mode_to_string(HOPFIELD_MODE_SPARSE), "SPARSE");
}

TEST_F(HopfieldMemoryTest, StoreModeToString) {
    EXPECT_STREQ(hopfield_store_mode_to_string(HOPFIELD_STORE_OVERWRITE), "OVERWRITE");
    EXPECT_STREQ(hopfield_store_mode_to_string(HOPFIELD_STORE_REJECT), "REJECT");
    EXPECT_STREQ(hopfield_store_mode_to_string(HOPFIELD_STORE_MERGE), "MERGE");
}

/* ============================================================================
 * Result Management Tests
 * ============================================================================ */

TEST_F(HopfieldMemoryTest, ResultCreateDestroy) {
    hopfield_retrieval_result_t* result = hopfield_result_create(64);
    ASSERT_NE(result, nullptr);
    ASSERT_NE(result->pattern, nullptr);

    hopfield_result_destroy(result);
    SUCCEED();
}

TEST_F(HopfieldMemoryTest, ResultDestroyNullIsSafe) {
    hopfield_result_destroy(nullptr);
    SUCCEED();
}

TEST_F(HopfieldMemoryTest, BatchResultCreateDestroy) {
    hopfield_batch_result_t* result = hopfield_batch_result_create(5, 64);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->num_results, 5);

    for (uint32_t i = 0; i < result->num_results; i++) {
        EXPECT_NE(result->results[i].pattern, nullptr);
    }

    hopfield_batch_result_destroy(result);
    SUCCEED();
}

/* ============================================================================
 * Bio-Async Tests
 * ============================================================================ */

TEST_F(HopfieldMemoryTest, ConnectBioAsyncSucceeds) {
    memory = hopfield_memory_create(&config);
    ASSERT_NE(memory, nullptr);

    EXPECT_EQ(hopfield_memory_connect_bio_async(memory), NIMCP_SUCCESS);
}

TEST_F(HopfieldMemoryTest, DisconnectBioAsyncSucceeds) {
    memory = hopfield_memory_create(&config);
    ASSERT_NE(memory, nullptr);

    EXPECT_EQ(hopfield_memory_disconnect_bio_async(memory), NIMCP_SUCCESS);
}

/* ============================================================================
 * Capacity Tests
 * ============================================================================ */

TEST_F(HopfieldMemoryTest, OverwriteWhenFull) {
    config.capacity = 5;
    config.store_mode = HOPFIELD_STORE_OVERWRITE;
    memory = hopfield_memory_create(&config);
    ASSERT_NE(memory, nullptr);

    float pattern[64];
    for (int i = 0; i < 10; i++) {
        FillPattern(pattern, (float)i);
        EXPECT_EQ(hopfield_memory_store(memory, pattern, nullptr), NIMCP_SUCCESS);
    }

    EXPECT_EQ(hopfield_memory_pattern_count(memory), 5);
}

TEST_F(HopfieldMemoryTest, RejectWhenFull) {
    config.capacity = 5;
    config.store_mode = HOPFIELD_STORE_REJECT;
    memory = hopfield_memory_create(&config);
    ASSERT_NE(memory, nullptr);

    float pattern[64];
    for (int i = 0; i < 5; i++) {
        FillPattern(pattern, (float)i);
        EXPECT_EQ(hopfield_memory_store(memory, pattern, nullptr), NIMCP_SUCCESS);
    }

    FillPattern(pattern, 5.0f);
    EXPECT_NE(hopfield_memory_store(memory, pattern, nullptr), NIMCP_SUCCESS);
    EXPECT_EQ(hopfield_memory_pattern_count(memory), 5);
}
