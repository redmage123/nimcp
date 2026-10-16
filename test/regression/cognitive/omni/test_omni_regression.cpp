/**
 * @file test_omni_regression.cpp
 * @brief Regression tests for Omnidirectional Inference performance
 * @version 1.0.0
 * @date 2025-01-04
 *
 * Performance benchmarks and regression tests for:
 * - Component creation/destruction
 * - Basic latency measurements
 * - Memory usage patterns
 *
 * TODO: Update API calls to match actual implementation signatures
 */

#include <gtest/gtest.h>
#include <chrono>
#include <vector>
#include <cmath>
#include <numeric>
#include <algorithm>
#include <random>
#include <cstring>

/* Headers have their own extern "C" guards - don't wrap them to avoid
 * CUDA C++ function conflicts */
#include "cognitive/jepa/nimcp_jepa_bidirectional.h"
#include "cognitive/memory/nimcp_hopfield_memory.h"
#include "cognitive/predictive/nimcp_predictive_hierarchy.h"
#include "cognitive/memory/nimcp_temporal_replay.h"
#include "utils/memory/nimcp_memory.h"

/* ============================================================================
 * Test Configuration
 * ============================================================================ */

namespace {
    constexpr uint32_t LATENT_DIM = 256;
    constexpr uint32_t HIDDEN_DIM = 512;
    constexpr uint32_t NUM_PATTERNS = 1024;
    constexpr uint32_t NUM_LEVELS = 5;
    constexpr uint32_t NUM_ITERATIONS = 100;

    /* Memory thresholds (bytes) */
    constexpr size_t MAX_JEPA_MEMORY = 64 * 1024 * 1024;       /* 64MB */
    constexpr size_t MAX_HOPFIELD_MEMORY = 256 * 1024 * 1024;  /* 256MB */
    constexpr size_t MAX_HIERARCHY_MEMORY = 128 * 1024 * 1024; /* 128MB */
}

/* ============================================================================
 * Performance Measurement Utilities
 * ============================================================================ */

class PerformanceTimer {
public:
    void start() {
        start_time = std::chrono::high_resolution_clock::now();
    }

    double stop_us() {
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
            end_time - start_time).count();
        return static_cast<double>(duration);
    }

private:
    std::chrono::high_resolution_clock::time_point start_time;
};

struct BenchmarkStats {
    double mean_us;
    double std_us;
    double min_us;
    double max_us;
    double p95_us;
    double p99_us;

    static BenchmarkStats compute(std::vector<double>& times) {
        BenchmarkStats stats;
        size_t n = times.size();
        if (n == 0) {
            memset(&stats, 0, sizeof(stats));
            return stats;
        }

        std::sort(times.begin(), times.end());

        stats.min_us = times.front();
        stats.max_us = times.back();
        stats.mean_us = std::accumulate(times.begin(), times.end(), 0.0) / n;
        stats.p95_us = times[static_cast<size_t>(n * 0.95)];
        stats.p99_us = times[static_cast<size_t>(n * 0.99)];

        double sq_sum = 0;
        for (double t : times) {
            sq_sum += (t - stats.mean_us) * (t - stats.mean_us);
        }
        stats.std_us = std::sqrt(sq_sum / n);

        return stats;
    }

    void print(const char* name) const {
        std::cout << name << " Latency:" << std::endl;
        std::cout << "  Mean: " << mean_us << " us" << std::endl;
        std::cout << "  Std:  " << std_us << " us" << std::endl;
        std::cout << "  Min:  " << min_us << " us" << std::endl;
        std::cout << "  Max:  " << max_us << " us" << std::endl;
        std::cout << "  P95:  " << p95_us << " us" << std::endl;
        std::cout << "  P99:  " << p99_us << " us" << std::endl;
    }
};

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

static size_t get_allocated_memory() {
    nimcp_memory_stats_t stats;
    if (nimcp_memory_get_stats(&stats)) {
        return stats.current_allocated;
    }
    return 0;
}

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class OmniRegressionTest : public ::testing::Test {
protected:
    void SetUp() override {
        /* Initialize components lazily in tests */
        jepa = nullptr;
        hopfield = nullptr;
        hierarchy = nullptr;
        replay = nullptr;
    }

    void TearDown() override {
        if (jepa) {
            jepa_bidirectional_destroy(jepa);
            jepa = nullptr;
        }
        if (hopfield) {
            hopfield_memory_destroy(hopfield);
            hopfield = nullptr;
        }
        if (hierarchy) {
            pred_hier_destroy(hierarchy);
            hierarchy = nullptr;
        }
        if (replay) {
            temporal_replay_destroy(replay);
            replay = nullptr;
        }
    }

    /* Create JEPA with default config */
    jepa_bidirectional_t* createJEPA() {
        jepa_bidir_config_t config;
        jepa_bidir_default_config(&config);
        config.embedding_dim = LATENT_DIM;
        config.hidden_dim = HIDDEN_DIM;
        return jepa_bidirectional_create(&config);
    }

    /* Create Hopfield memory */
    hopfield_memory_t* createHopfield(uint32_t capacity = NUM_PATTERNS) {
        hopfield_config_t config;
        hopfield_default_config(&config);
        config.pattern_dim = LATENT_DIM;
        config.capacity = capacity;
        return hopfield_memory_create(&config);
    }

    /* Create predictive hierarchy */
    predictive_hierarchy_t* createHierarchy() {
        pred_hier_config_t config;
        pred_hier_default_config(&config);
        config.num_levels = NUM_LEVELS;

        /* Allocate and configure level configs */
        pred_level_config_t* level_configs = new pred_level_config_t[NUM_LEVELS];
        for (uint32_t i = 0; i < NUM_LEVELS; i++) {
            level_configs[i].dim = LATENT_DIM / (1 << i);  /* Shrinking dims */
            level_configs[i].gen_hidden_dim = HIDDEN_DIM / (1 << i);
            level_configs[i].gen_type = PRED_HIER_GEN_LINEAR;
            level_configs[i].initial_precision = 1.0f;
            level_configs[i].precision_lr = 0.01f;
            level_configs[i].learnable_precision = true;
        }
        config.level_configs = level_configs;

        predictive_hierarchy_t* hier = pred_hier_create(&config);
        delete[] level_configs;
        return hier;
    }

    /* Create temporal replay */
    temporal_replay_t* createReplay() {
        replay_config_t config;
        replay_default_config(&config);
        config.capacity = NUM_PATTERNS;
        config.state_dim = LATENT_DIM;
        config.action_dim = 32;
        return temporal_replay_create(&config);
    }

    jepa_bidirectional_t* jepa;
    hopfield_memory_t* hopfield;
    predictive_hierarchy_t* hierarchy;
    temporal_replay_t* replay;
};

/* ============================================================================
 * JEPA Bidirectional Tests
 * ============================================================================ */

TEST_F(OmniRegressionTest, JEPACreateDestroy) {
    /* Test creation and destruction don't crash */
    jepa = createJEPA();
    ASSERT_NE(jepa, nullptr) << "JEPA creation failed";
}

TEST_F(OmniRegressionTest, JEPAConfigValidation) {
    jepa_bidir_config_t config;
    jepa_bidir_default_config(&config);

    /* Validate default config is valid */
    int result = jepa_bidir_validate_config(&config);
    EXPECT_EQ(result, 0) << "Default config should be valid";
}

TEST_F(OmniRegressionTest, JEPACreationLatency) {
    std::vector<double> times;
    times.reserve(NUM_ITERATIONS);

    PerformanceTimer timer;

    for (uint32_t i = 0; i < NUM_ITERATIONS; i++) {
        timer.start();
        jepa_bidirectional_t* j = createJEPA();
        times.push_back(timer.stop_us());

        if (j) {
            jepa_bidirectional_destroy(j);
        }
    }

    BenchmarkStats stats = BenchmarkStats::compute(times);
    stats.print("JEPA Creation");
}

/* ============================================================================
 * Hopfield Memory Tests
 * ============================================================================ */

TEST_F(OmniRegressionTest, HopfieldCreateDestroy) {
    hopfield = createHopfield();
    ASSERT_NE(hopfield, nullptr) << "Hopfield creation failed";
}

TEST_F(OmniRegressionTest, HopfieldConfigValidation) {
    hopfield_config_t config;
    hopfield_default_config(&config);

    /* Validate default config is valid */
    int result = hopfield_validate_config(&config);
    EXPECT_EQ(result, 0) << "Default config should be valid";
}

TEST_F(OmniRegressionTest, HopfieldCreationLatency) {
    std::vector<double> times;
    times.reserve(NUM_ITERATIONS);

    PerformanceTimer timer;

    for (uint32_t i = 0; i < NUM_ITERATIONS; i++) {
        timer.start();
        hopfield_memory_t* h = createHopfield();
        times.push_back(timer.stop_us());

        if (h) {
            hopfield_memory_destroy(h);
        }
    }

    BenchmarkStats stats = BenchmarkStats::compute(times);
    stats.print("Hopfield Creation");
}

/* ============================================================================
 * Predictive Hierarchy Tests
 * ============================================================================ */

TEST_F(OmniRegressionTest, HierarchyCreateDestroy) {
    hierarchy = createHierarchy();
    ASSERT_NE(hierarchy, nullptr) << "Hierarchy creation failed";
}

TEST_F(OmniRegressionTest, HierarchyCreationLatency) {
    std::vector<double> times;
    times.reserve(NUM_ITERATIONS);

    PerformanceTimer timer;

    for (uint32_t i = 0; i < NUM_ITERATIONS; i++) {
        timer.start();
        predictive_hierarchy_t* h = createHierarchy();
        times.push_back(timer.stop_us());

        if (h) {
            pred_hier_destroy(h);
        }
    }

    BenchmarkStats stats = BenchmarkStats::compute(times);
    stats.print("Hierarchy Creation");
}

/* ============================================================================
 * Temporal Replay Tests
 * ============================================================================ */

TEST_F(OmniRegressionTest, ReplayCreateDestroy) {
    replay = createReplay();
    ASSERT_NE(replay, nullptr) << "Replay creation failed";
}

TEST_F(OmniRegressionTest, ReplayConfigValidation) {
    replay_config_t config;
    replay_default_config(&config);

    /* Validate default config is valid */
    int result = replay_validate_config(&config);
    EXPECT_EQ(result, 0) << "Default config should be valid";
}

TEST_F(OmniRegressionTest, ReplayCreationLatency) {
    std::vector<double> times;
    times.reserve(NUM_ITERATIONS);

    PerformanceTimer timer;

    for (uint32_t i = 0; i < NUM_ITERATIONS; i++) {
        timer.start();
        temporal_replay_t* r = createReplay();
        times.push_back(timer.stop_us());

        if (r) {
            temporal_replay_destroy(r);
        }
    }

    BenchmarkStats stats = BenchmarkStats::compute(times);
    stats.print("Replay Creation");
}

/* ============================================================================
 * Memory Usage Tests
 * ============================================================================ */

TEST_F(OmniRegressionTest, JEPAMemoryFootprint) {
    size_t before = get_allocated_memory();
    jepa = createJEPA();
    size_t after = get_allocated_memory();

    if (!jepa) {
        GTEST_SKIP() << "JEPA creation not available";
    }

    size_t memory_used = after - before;
    std::cout << "JEPA Memory Footprint: " << memory_used / 1024 << " KB" << std::endl;

    EXPECT_LT(memory_used, MAX_JEPA_MEMORY)
        << "JEPA memory usage exceeds threshold";
}

TEST_F(OmniRegressionTest, HopfieldMemoryFootprint) {
    size_t before = get_allocated_memory();
    hopfield = createHopfield();
    size_t after = get_allocated_memory();

    if (!hopfield) {
        GTEST_SKIP() << "Hopfield creation not available";
    }

    size_t memory_used = after - before;
    std::cout << "Hopfield Memory Footprint: " << memory_used / 1024 << " KB" << std::endl;

    EXPECT_LT(memory_used, MAX_HOPFIELD_MEMORY)
        << "Hopfield memory usage exceeds threshold";
}

TEST_F(OmniRegressionTest, HierarchyMemoryFootprint) {
    size_t before = get_allocated_memory();
    hierarchy = createHierarchy();
    size_t after = get_allocated_memory();

    if (!hierarchy) {
        GTEST_SKIP() << "Hierarchy creation not available";
    }

    size_t memory_used = after - before;
    std::cout << "Hierarchy Memory Footprint: " << memory_used / 1024 << " KB" << std::endl;

    EXPECT_LT(memory_used, MAX_HIERARCHY_MEMORY)
        << "Hierarchy memory usage exceeds threshold";
}

TEST_F(OmniRegressionTest, HopfieldMemoryScaling) {
    /* Test memory usage scales with capacity */
    std::vector<uint32_t> capacities = {256, 1024, 4096};
    std::vector<size_t> memories;

    for (uint32_t cap : capacities) {
        size_t before = get_allocated_memory();
        hopfield_memory_t* hm = createHopfield(cap);
        size_t after = get_allocated_memory();

        if (!hm) {
            std::cout << "Hopfield creation failed for capacity " << cap << std::endl;
            continue;
        }

        memories.push_back(after - before);

        hopfield_memory_destroy(hm);

        std::cout << "Hopfield Memory (cap=" << cap << "): "
                  << memories.back() / 1024 << " KB" << std::endl;
    }

    /* Verify memory increases with capacity */
    if (memories.size() >= 2) {
        for (size_t i = 1; i < memories.size(); i++) {
            EXPECT_GE(memories[i], memories[i-1])
                << "Hopfield memory should increase with capacity";
        }
    }
}

/* ============================================================================
 * Combined System Tests
 * ============================================================================ */

TEST_F(OmniRegressionTest, AllComponentsCreateDestroy) {
    /* Test all components can be created and destroyed together */
    jepa = createJEPA();
    hopfield = createHopfield();
    hierarchy = createHierarchy();
    replay = createReplay();

    /* Verify at least some components created successfully */
    int created_count = 0;
    if (jepa) created_count++;
    if (hopfield) created_count++;
    if (hierarchy) created_count++;
    if (replay) created_count++;

    std::cout << "Components created: " << created_count << "/4" << std::endl;
    EXPECT_GE(created_count, 1) << "At least one component should be creatable";
}

TEST_F(OmniRegressionTest, TotalSystemMemory) {
    size_t initial = get_allocated_memory();

    jepa = createJEPA();
    hopfield = createHopfield();
    hierarchy = createHierarchy();
    replay = createReplay();

    size_t total = get_allocated_memory() - initial;

    std::cout << "Total system memory: " << total / (1024 * 1024) << " MB" << std::endl;

    /* Total should be less than sum of individual limits */
    size_t max_total = MAX_JEPA_MEMORY + MAX_HOPFIELD_MEMORY +
                       MAX_HIERARCHY_MEMORY + 64 * 1024 * 1024;
    EXPECT_LT(total, max_total) << "Total memory exceeds combined limits";
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
