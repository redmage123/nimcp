/**
 * @file test_mesh_timing.cpp
 * @brief Unit Tests for Hierarchical Pink Noise Timing
 *
 * Tests: Configuration, interval generation, heartbeat/timeout,
 *        adaptive timing, statistics, and pink noise validation.
 *
 * @author NIMCP Development Team
 * @date 2025-01-31
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <numeric>

extern "C" {
#include "mesh/nimcp_mesh_timing.h"
#include "mesh/nimcp_mesh_types.h"
}

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class MeshTimingTest : public ::testing::Test {
protected:
    mesh_hierarchical_timing_t timing;

    void SetUp() override {
        timing = mesh_timing_create(nullptr);
        ASSERT_NE(timing, nullptr);
    }

    void TearDown() override {
        mesh_timing_destroy(timing);
        timing = nullptr;
    }

    /* Helper: Generate many samples and compute statistics */
    struct SampleStats {
        float mean;
        float std;
        float min;
        float max;
    };

    SampleStats generate_samples(mesh_timing_level_t level, size_t count) {
        SampleStats stats = {0, 0, FLT_MAX, -FLT_MAX};
        std::vector<float> samples(count);

        for (size_t i = 0; i < count; i++) {
            samples[i] = mesh_timing_next_interval(timing, level);
            if (samples[i] < stats.min) stats.min = samples[i];
            if (samples[i] > stats.max) stats.max = samples[i];
        }

        stats.mean = std::accumulate(samples.begin(), samples.end(), 0.0f) / count;

        float variance = 0.0f;
        for (float s : samples) {
            variance += (s - stats.mean) * (s - stats.mean);
        }
        stats.std = std::sqrt(variance / count);

        return stats;
    }
};

/* ============================================================================
 * Default Configuration Tests
 * ============================================================================ */

TEST_F(MeshTimingTest, DefaultConfigHasSensibleValues) {
    mesh_hierarchical_timing_config_t config = mesh_timing_default_config();

    /* Check system level */
    EXPECT_FLOAT_EQ(config.levels[MESH_TIMING_LEVEL_SYSTEM].base_interval_ms,
                    MESH_TIMING_SYSTEM_BASE_MS);
    EXPECT_FLOAT_EQ(config.levels[MESH_TIMING_LEVEL_SYSTEM].jitter_amplitude_ms,
                    MESH_TIMING_SYSTEM_JITTER_MS);

    /* Check ordering level (fastest) */
    EXPECT_FLOAT_EQ(config.levels[MESH_TIMING_LEVEL_ORDERING].base_interval_ms,
                    MESH_TIMING_ORDERING_BASE_MS);

    /* Hierarchy: system > hemisphere > layer > ordering */
    EXPECT_GT(config.levels[MESH_TIMING_LEVEL_SYSTEM].base_interval_ms,
              config.levels[MESH_TIMING_LEVEL_HEMISPHERE].base_interval_ms);
    EXPECT_GT(config.levels[MESH_TIMING_LEVEL_HEMISPHERE].base_interval_ms,
              config.levels[MESH_TIMING_LEVEL_LAYER].base_interval_ms);
    EXPECT_GT(config.levels[MESH_TIMING_LEVEL_LAYER].base_interval_ms,
              config.levels[MESH_TIMING_LEVEL_ORDERING].base_interval_ms);
}

TEST_F(MeshTimingTest, DefaultLevelConfigMatchesFull) {
    mesh_hierarchical_timing_config_t full = mesh_timing_default_config();

    for (int i = 0; i < MESH_TIMING_NUM_LEVELS; i++) {
        mesh_timing_level_config_t level_cfg = mesh_timing_default_level_config((mesh_timing_level_t)i);
        EXPECT_FLOAT_EQ(level_cfg.base_interval_ms, full.levels[i].base_interval_ms);
        EXPECT_FLOAT_EQ(level_cfg.jitter_amplitude_ms, full.levels[i].jitter_amplitude_ms);
    }
}

TEST_F(MeshTimingTest, CreateWithCustomConfig) {
    mesh_hierarchical_timing_config_t config = mesh_timing_default_config();
    config.levels[MESH_TIMING_LEVEL_LAYER].base_interval_ms = 20.0f;
    config.random_seed = 12345;

    mesh_hierarchical_timing_t custom = mesh_timing_create(&config);
    ASSERT_NE(custom, nullptr);

    /* Should generate intervals around new base */
    float interval = mesh_timing_next_interval(custom, MESH_TIMING_LEVEL_LAYER);
    EXPECT_GT(interval, 0.0f);

    mesh_timing_destroy(custom);
}

/* ============================================================================
 * Interval Generation Tests
 * ============================================================================ */

TEST_F(MeshTimingTest, GenerateIntervalSystem) {
    float interval = mesh_timing_next_interval(timing, MESH_TIMING_LEVEL_SYSTEM);

    EXPECT_GE(interval, MESH_TIMING_SYSTEM_MIN_MS);
    EXPECT_LE(interval, MESH_TIMING_SYSTEM_MAX_MS);
}

TEST_F(MeshTimingTest, GenerateIntervalHemisphere) {
    float interval = mesh_timing_next_interval(timing, MESH_TIMING_LEVEL_HEMISPHERE);

    EXPECT_GE(interval, MESH_TIMING_HEMISPHERE_MIN_MS);
    EXPECT_LE(interval, MESH_TIMING_HEMISPHERE_MAX_MS);
}

TEST_F(MeshTimingTest, GenerateIntervalLayer) {
    float interval = mesh_timing_next_interval(timing, MESH_TIMING_LEVEL_LAYER);

    EXPECT_GE(interval, MESH_TIMING_LAYER_MIN_MS);
    EXPECT_LE(interval, MESH_TIMING_LAYER_MAX_MS);
}

TEST_F(MeshTimingTest, GenerateIntervalOrdering) {
    float interval = mesh_timing_next_interval(timing, MESH_TIMING_LEVEL_ORDERING);

    EXPECT_GE(interval, MESH_TIMING_ORDERING_MIN_MS);
    EXPECT_LE(interval, MESH_TIMING_ORDERING_MAX_MS);
}

TEST_F(MeshTimingTest, GenerateIntervalInvalidLevel) {
    /* Should return safe default for invalid level */
    float interval = mesh_timing_next_interval(timing, (mesh_timing_level_t)99);
    EXPECT_GT(interval, 0.0f);
}

TEST_F(MeshTimingTest, GenerateIntervalNanoseconds) {
    uint64_t ns = mesh_timing_next_interval_ns(timing, MESH_TIMING_LEVEL_LAYER);

    /* Should be in valid range */
    uint64_t min_ns = (uint64_t)(MESH_TIMING_LAYER_MIN_MS * 1000000.0f);
    uint64_t max_ns = (uint64_t)(MESH_TIMING_LAYER_MAX_MS * 1000000.0f);

    EXPECT_GE(ns, min_ns);
    EXPECT_LE(ns, max_ns);
}

TEST_F(MeshTimingTest, GenerateBatch) {
    const size_t count = 100;
    std::vector<float> intervals(count);

    ASSERT_EQ(mesh_timing_generate_batch(timing, MESH_TIMING_LEVEL_LAYER, intervals.data(), count),
              NIMCP_SUCCESS);

    /* All should be in valid range */
    for (float interval : intervals) {
        EXPECT_GE(interval, MESH_TIMING_LAYER_MIN_MS);
        EXPECT_LE(interval, MESH_TIMING_LAYER_MAX_MS);
    }
}

TEST_F(MeshTimingTest, IntervalsMeanNearBase) {
    /* Generate many samples and check mean is near base */
    SampleStats stats = generate_samples(MESH_TIMING_LEVEL_LAYER, 1000);

    /* Mean should be within 50% of base */
    EXPECT_NEAR(stats.mean, MESH_TIMING_LAYER_BASE_MS, MESH_TIMING_LAYER_BASE_MS * 0.5f);
}

TEST_F(MeshTimingTest, IntervalsHaveVariation) {
    /* Generate samples and verify there is jitter */
    SampleStats stats = generate_samples(MESH_TIMING_LEVEL_LAYER, 1000);

    /* Should have non-trivial standard deviation */
    EXPECT_GT(stats.std, 0.1f);
}

TEST_F(MeshTimingTest, IntervalsClamped) {
    /* Generate many samples - all should be within [min, max] */
    for (int i = 0; i < 10000; i++) {
        float interval = mesh_timing_next_interval(timing, MESH_TIMING_LEVEL_LAYER);
        EXPECT_GE(interval, MESH_TIMING_LAYER_MIN_MS);
        EXPECT_LE(interval, MESH_TIMING_LAYER_MAX_MS);
    }
}

/* ============================================================================
 * Heartbeat and Timeout Tests
 * ============================================================================ */

TEST_F(MeshTimingTest, HeartbeatIntervalInRange) {
    float heartbeat = mesh_timing_heartbeat_interval(timing, MESH_TIMING_LEVEL_LAYER);

    EXPECT_GE(heartbeat, MESH_TIMING_LAYER_MIN_MS);
    EXPECT_LE(heartbeat, MESH_TIMING_LAYER_MAX_MS);
}

TEST_F(MeshTimingTest, ElectionTimeoutGreaterThanHeartbeat) {
    /* Election timeout should be 2-3x heartbeat */
    float heartbeat_base = MESH_TIMING_LAYER_BASE_MS;
    float timeout = mesh_timing_election_timeout(timing, MESH_TIMING_LEVEL_LAYER);

    EXPECT_GE(timeout, heartbeat_base * 1.5f);
    EXPECT_LE(timeout, heartbeat_base * 4.0f);
}

TEST_F(MeshTimingTest, TransactionTimeout) {
    float base = 100.0f;
    float timeout = mesh_timing_transaction_timeout(timing, MESH_TIMING_LEVEL_LAYER, base);

    /* Should be close to base with some jitter */
    EXPECT_GE(timeout, base * 0.5f);
    EXPECT_LE(timeout, base * 2.0f);
}

TEST_F(MeshTimingTest, ElectionTimeoutsVary) {
    /* Multiple election timeouts should vary (randomized) */
    std::set<float> timeouts;
    for (int i = 0; i < 100; i++) {
        float timeout = mesh_timing_election_timeout(timing, MESH_TIMING_LEVEL_LAYER);
        timeouts.insert(timeout);
    }

    /* Should have multiple distinct values */
    EXPECT_GT(timeouts.size(), 10u);
}

/* ============================================================================
 * Adaptive Timing Tests
 * ============================================================================ */

TEST_F(MeshTimingTest, AdaptationDisabledByDefault) {
    float initial_factor = mesh_timing_get_adaptation_factor(timing, MESH_TIMING_LEVEL_LAYER);
    EXPECT_FLOAT_EQ(initial_factor, 1.0f);

    /* Report latency - should not change factor since adaptation is disabled */
    mesh_timing_report_latency(timing, MESH_TIMING_LEVEL_LAYER, 50.0f);

    float after_factor = mesh_timing_get_adaptation_factor(timing, MESH_TIMING_LEVEL_LAYER);
    EXPECT_FLOAT_EQ(after_factor, 1.0f);
}

TEST_F(MeshTimingTest, AdaptationWhenEnabled) {
    mesh_hierarchical_timing_config_t config = mesh_timing_default_config();
    config.enable_adaptation = true;
    config.adaptation_rate = 0.3f;

    mesh_hierarchical_timing_t adaptive = mesh_timing_create(&config);
    ASSERT_NE(adaptive, nullptr);

    /* Report high latency repeatedly */
    for (int i = 0; i < 10; i++) {
        mesh_timing_report_latency(adaptive, MESH_TIMING_LEVEL_LAYER, 30.0f);  /* 3x base */
    }

    float factor = mesh_timing_get_adaptation_factor(adaptive, MESH_TIMING_LEVEL_LAYER);
    EXPECT_GT(factor, 1.0f);  /* Should have increased */

    mesh_timing_destroy(adaptive);
}

TEST_F(MeshTimingTest, ResetAdaptationSingleLevel) {
    mesh_hierarchical_timing_config_t config = mesh_timing_default_config();
    config.enable_adaptation = true;

    mesh_hierarchical_timing_t adaptive = mesh_timing_create(&config);

    /* Modify adaptation */
    for (int i = 0; i < 5; i++) {
        mesh_timing_report_latency(adaptive, MESH_TIMING_LEVEL_LAYER, 50.0f);
    }

    /* Reset single level */
    ASSERT_EQ(mesh_timing_reset_adaptation(adaptive, MESH_TIMING_LEVEL_LAYER), NIMCP_SUCCESS);

    float factor = mesh_timing_get_adaptation_factor(adaptive, MESH_TIMING_LEVEL_LAYER);
    EXPECT_FLOAT_EQ(factor, 1.0f);

    mesh_timing_destroy(adaptive);
}

TEST_F(MeshTimingTest, ResetAdaptationAllLevels) {
    mesh_hierarchical_timing_config_t config = mesh_timing_default_config();
    config.enable_adaptation = true;

    mesh_hierarchical_timing_t adaptive = mesh_timing_create(&config);

    /* Modify all levels */
    for (int level = 0; level < MESH_TIMING_NUM_LEVELS; level++) {
        for (int i = 0; i < 5; i++) {
            mesh_timing_report_latency(adaptive, (mesh_timing_level_t)level, 100.0f);
        }
    }

    /* Reset all by passing invalid level */
    ASSERT_EQ(mesh_timing_reset_adaptation(adaptive, (mesh_timing_level_t)MESH_TIMING_NUM_LEVELS), NIMCP_SUCCESS);

    for (int level = 0; level < MESH_TIMING_NUM_LEVELS; level++) {
        float factor = mesh_timing_get_adaptation_factor(adaptive, (mesh_timing_level_t)level);
        EXPECT_FLOAT_EQ(factor, 1.0f);
    }

    mesh_timing_destroy(adaptive);
}

/* ============================================================================
 * Configuration Update Tests
 * ============================================================================ */

TEST_F(MeshTimingTest, UpdateLevelConfig) {
    mesh_timing_level_config_t new_config = {
        .base_interval_ms = 50.0f,
        .jitter_amplitude_ms = 10.0f,
        .min_interval_ms = 40.0f,
        .max_interval_ms = 60.0f,
        .pink_alpha = 1.0f
    };

    ASSERT_EQ(mesh_timing_update_level_config(timing, MESH_TIMING_LEVEL_LAYER, &new_config), NIMCP_SUCCESS);

    /* Generate samples - should be in new range */
    for (int i = 0; i < 100; i++) {
        float interval = mesh_timing_next_interval(timing, MESH_TIMING_LEVEL_LAYER);
        EXPECT_GE(interval, 40.0f);
        EXPECT_LE(interval, 60.0f);
    }
}

TEST_F(MeshTimingTest, Reseed) {
    ASSERT_EQ(mesh_timing_reseed(timing, 12345), NIMCP_SUCCESS);

    /* Generate some samples */
    float sample1 = mesh_timing_next_interval(timing, MESH_TIMING_LEVEL_LAYER);

    /* Reseed with same seed */
    ASSERT_EQ(mesh_timing_reseed(timing, 12345), NIMCP_SUCCESS);

    /* Should get same sequence */
    float sample2 = mesh_timing_next_interval(timing, MESH_TIMING_LEVEL_LAYER);

    EXPECT_FLOAT_EQ(sample1, sample2);
}

/* ============================================================================
 * Statistics Tests
 * ============================================================================ */

TEST_F(MeshTimingTest, GetStatsAllLevels) {
    /* Generate samples for all levels */
    for (int level = 0; level < MESH_TIMING_NUM_LEVELS; level++) {
        for (int i = 0; i < 100; i++) {
            mesh_timing_next_interval(timing, (mesh_timing_level_t)level);
        }
    }

    mesh_hierarchical_timing_stats_t stats;
    ASSERT_EQ(mesh_timing_get_stats(timing, &stats), NIMCP_SUCCESS);

    EXPECT_EQ(stats.total_samples, 400u);

    for (int level = 0; level < MESH_TIMING_NUM_LEVELS; level++) {
        EXPECT_EQ(stats.levels[level].samples_generated, 100u);
        EXPECT_GT(stats.levels[level].mean_interval_ms, 0.0f);
    }
}

TEST_F(MeshTimingTest, GetLevelStats) {
    /* Generate samples for one level */
    for (int i = 0; i < 50; i++) {
        mesh_timing_next_interval(timing, MESH_TIMING_LEVEL_HEMISPHERE);
    }

    mesh_timing_level_stats_t stats;
    ASSERT_EQ(mesh_timing_get_level_stats(timing, MESH_TIMING_LEVEL_HEMISPHERE, &stats), NIMCP_SUCCESS);

    EXPECT_EQ(stats.samples_generated, 50u);
    EXPECT_NEAR(stats.mean_interval_ms, MESH_TIMING_HEMISPHERE_BASE_MS,
                MESH_TIMING_HEMISPHERE_JITTER_MS * 2.0f);
}

TEST_F(MeshTimingTest, ResetStats) {
    /* Generate some samples */
    for (int i = 0; i < 100; i++) {
        mesh_timing_next_interval(timing, MESH_TIMING_LEVEL_LAYER);
    }

    ASSERT_EQ(mesh_timing_reset_stats(timing), NIMCP_SUCCESS);

    mesh_timing_level_stats_t stats;
    ASSERT_EQ(mesh_timing_get_level_stats(timing, MESH_TIMING_LEVEL_LAYER, &stats), NIMCP_SUCCESS);

    EXPECT_EQ(stats.samples_generated, 0u);
}

/* ============================================================================
 * Pink Noise Validation Tests
 * ============================================================================ */

TEST_F(MeshTimingTest, ValidatePinkNoise) {
    /* Test that generated noise has expected properties */
    bool valid = mesh_timing_validate_pink_noise(timing, MESH_TIMING_LEVEL_LAYER, 256);
    EXPECT_TRUE(valid);
}

TEST_F(MeshTimingTest, ValidatePinkNoiseMinSamples) {
    /* Should use minimum samples if too few specified */
    bool valid = mesh_timing_validate_pink_noise(timing, MESH_TIMING_LEVEL_LAYER, 10);
    EXPECT_TRUE(valid);  /* Will use 64 minimum samples */
}

/* ============================================================================
 * Utility Function Tests
 * ============================================================================ */

TEST_F(MeshTimingTest, LevelToString) {
    EXPECT_STREQ(mesh_timing_level_to_string(MESH_TIMING_LEVEL_SYSTEM), "SYSTEM");
    EXPECT_STREQ(mesh_timing_level_to_string(MESH_TIMING_LEVEL_HEMISPHERE), "HEMISPHERE");
    EXPECT_STREQ(mesh_timing_level_to_string(MESH_TIMING_LEVEL_LAYER), "LAYER");
    EXPECT_STREQ(mesh_timing_level_to_string(MESH_TIMING_LEVEL_ORDERING), "ORDERING");
    EXPECT_STREQ(mesh_timing_level_to_string((mesh_timing_level_t)99), "UNKNOWN");
}

TEST_F(MeshTimingTest, LevelFromCoordLevel) {
    EXPECT_EQ(mesh_timing_level_from_coord_level(0), MESH_TIMING_LEVEL_SYSTEM);
    EXPECT_EQ(mesh_timing_level_from_coord_level(1), MESH_TIMING_LEVEL_HEMISPHERE);
    EXPECT_EQ(mesh_timing_level_from_coord_level(2), MESH_TIMING_LEVEL_LAYER);
    EXPECT_EQ(mesh_timing_level_from_coord_level(3), MESH_TIMING_LEVEL_ORDERING);
    EXPECT_EQ(mesh_timing_level_from_coord_level(99), MESH_TIMING_LEVEL_LAYER);  /* Default */
}

TEST_F(MeshTimingTest, ExpectedConvergence) {
    float convergence = mesh_timing_expected_convergence(timing, MESH_TIMING_LEVEL_LAYER, 100);

    /* Should be O(log N) * interval * safety_factor */
    float expected_rounds = std::log2(100.0f);  /* ~6.6 */
    float expected_base = expected_rounds * MESH_TIMING_LAYER_BASE_MS * 1.5f;

    /* Should be in reasonable range */
    EXPECT_GT(convergence, expected_rounds * MESH_TIMING_LAYER_BASE_MS * 0.5f);
    EXPECT_LT(convergence, expected_rounds * MESH_TIMING_LAYER_BASE_MS * 5.0f);
}

TEST_F(MeshTimingTest, ExpectedConvergenceSmallGroup) {
    float convergence = mesh_timing_expected_convergence(timing, MESH_TIMING_LEVEL_LAYER, 2);

    /* Minimum 1 round */
    EXPECT_GT(convergence, MESH_TIMING_LAYER_BASE_MS);
}

/* ============================================================================
 * Debug Output Test
 * ============================================================================ */

TEST_F(MeshTimingTest, PrintDebugDoesNotCrash) {
    /* Generate some samples first */
    for (int i = 0; i < 100; i++) {
        mesh_timing_next_interval(timing, MESH_TIMING_LEVEL_LAYER);
    }

    mesh_timing_print_debug(timing);  /* Should not crash */
    mesh_timing_print_debug(nullptr);  /* Should handle NULL */
}

/* ============================================================================
 * Null Handling Tests
 * ============================================================================ */

TEST_F(MeshTimingTest, NullContextHandling) {
    EXPECT_GT(mesh_timing_next_interval(nullptr, MESH_TIMING_LEVEL_LAYER), 0.0f);

    float intervals[10];
    EXPECT_EQ(mesh_timing_generate_batch(nullptr, MESH_TIMING_LEVEL_LAYER, intervals, 10),
              NIMCP_ERROR_INVALID_PARAMETER);

    EXPECT_EQ(mesh_timing_report_latency(nullptr, MESH_TIMING_LEVEL_LAYER, 10.0f),
              NIMCP_ERROR_INVALID_PARAMETER);

    EXPECT_EQ(mesh_timing_reset_adaptation(nullptr, MESH_TIMING_LEVEL_LAYER),
              NIMCP_ERROR_INVALID_PARAMETER);
}

TEST_F(MeshTimingTest, NullOutputHandling) {
    EXPECT_EQ(mesh_timing_generate_batch(timing, MESH_TIMING_LEVEL_LAYER, nullptr, 10),
              NIMCP_ERROR_INVALID_PARAMETER);

    EXPECT_EQ(mesh_timing_get_stats(timing, nullptr), NIMCP_ERROR_INVALID_PARAMETER);
    EXPECT_EQ(mesh_timing_get_level_stats(timing, MESH_TIMING_LEVEL_LAYER, nullptr),
              NIMCP_ERROR_INVALID_PARAMETER);
}

TEST_F(MeshTimingTest, NullConfigUpdate) {
    EXPECT_EQ(mesh_timing_update_level_config(timing, MESH_TIMING_LEVEL_LAYER, nullptr),
              NIMCP_ERROR_INVALID_PARAMETER);
}

/* ============================================================================
 * Timing Hierarchy Tests
 * ============================================================================ */

TEST_F(MeshTimingTest, HierarchyMeansDecrease) {
    /* System level should have highest mean, ordering lowest */
    SampleStats system_stats = generate_samples(MESH_TIMING_LEVEL_SYSTEM, 500);
    SampleStats hemisphere_stats = generate_samples(MESH_TIMING_LEVEL_HEMISPHERE, 500);
    SampleStats layer_stats = generate_samples(MESH_TIMING_LEVEL_LAYER, 500);
    SampleStats ordering_stats = generate_samples(MESH_TIMING_LEVEL_ORDERING, 500);

    EXPECT_GT(system_stats.mean, hemisphere_stats.mean);
    EXPECT_GT(hemisphere_stats.mean, layer_stats.mean);
    EXPECT_GT(layer_stats.mean, ordering_stats.mean);
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
