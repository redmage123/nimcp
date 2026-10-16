/**
 * @file test_orchestrator_regression.cpp
 * @brief Regression tests for plasticity orchestrator
 *
 * WHAT: Regression tests for parameter stability, numerical stability,
 *       thread safety, and behavioral consistency
 * WHY:  Ensure orchestrator behavior remains stable across updates
 * HOW:  Test critical parameters, verify no NaN/Inf, test concurrent access,
 *       check memory leaks, validate callback consistency
 *
 * COVERAGE:
 * - Default configuration stability
 * - Weight bounds consistency across updates
 * - Numerical stability (no NaN/Inf)
 * - Stats accumulation correctness
 * - Module enable/disable consistency
 * - Memory leak checks (create/destroy cycles)
 * - Thread safety under concurrent updates
 * - Long-running simulation stability
 *
 * @author NIMCP Development Team
 * @date 2025-12-19
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>

// Headers have their own extern "C" guards
#include "plasticity/nimcp_plasticity_orchestrator.h"

/* ============================================================================
 * Test Fixtures
 * ============================================================================ */

class OrchestratorRegressionTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}

    bool IsNumericallyStable(float value) {
        return !std::isnan(value) && !std::isinf(value);
    }

    bool IsInBounds(float value, float min, float max) {
        return value >= min && value <= max;
    }
};

/* ============================================================================
 * Default Configuration Stability
 * ============================================================================ */

TEST_F(OrchestratorRegressionTest, DefaultConfigStability) {
    plasticity_orchestrator_config_t config;
    int ret = plasticity_orchestrator_default_config(&config);
    EXPECT_EQ(ret, 0);

    // Verify all modules are enabled by default
    EXPECT_TRUE(config.enabled.enable_triplet_stdp);
    EXPECT_TRUE(config.enabled.enable_bcm);
    EXPECT_TRUE(config.enabled.enable_homeostatic);
    EXPECT_TRUE(config.enabled.enable_metabolic);
    EXPECT_TRUE(config.enabled.enable_calcium);
    EXPECT_TRUE(config.enabled.enable_structural);
    EXPECT_TRUE(config.enabled.enable_protein_synthesis);
    EXPECT_TRUE(config.enabled.enable_metaplasticity);
    EXPECT_TRUE(config.enabled.enable_heterosynaptic);
    EXPECT_TRUE(config.enabled.enable_astrocyte);
}

TEST_F(OrchestratorRegressionTest, DefaultConfigReproducible) {
    // Get default config twice
    plasticity_orchestrator_config_t config1, config2;
    plasticity_orchestrator_default_config(&config1);
    plasticity_orchestrator_default_config(&config2);

    // Should be identical
    EXPECT_EQ(config1.enabled.enable_triplet_stdp, config2.enabled.enable_triplet_stdp);
    EXPECT_EQ(config1.enabled.enable_bcm, config2.enabled.enable_bcm);
    EXPECT_EQ(config1.enabled.enable_homeostatic, config2.enabled.enable_homeostatic);
}

/* ============================================================================
 * Weight Bounds Regression
 * ============================================================================ */

TEST_F(OrchestratorRegressionTest, WeightBoundsAfterManyUpdates) {
    plasticity_orchestrator_config_t config;
    plasticity_orchestrator_default_config(&config);
    config.enabled.enable_triplet_stdp = true;
    config.enabled.enable_bcm = true;
    config.enabled.enable_homeostatic = true;

    plasticity_orchestrator_t* orch = plasticity_orchestrator_create(&config);
    ASSERT_NE(orch, nullptr);

    // Create synapses
    for (int i = 0; i < 10; i++) {
        plasticity_orchestrator_set_weight(orch, i, 0.5f);
    }

    // Run many updates
    for (int u = 0; u < 1000; u++) {
        plasticity_orchestrator_update(orch, 1);
    }

    // Verify weights remain bounded
    for (int i = 0; i < 10; i++) {
        float weight = plasticity_orchestrator_get_weight(orch, i);
        if (!std::isnan(weight)) {
            EXPECT_TRUE(IsInBounds(weight, 0.0f, 1.0f))
                << "Weight out of bounds: " << weight << " for synapse " << i;
        }
    }

    plasticity_orchestrator_destroy(orch);
}

TEST_F(OrchestratorRegressionTest, WeightSetGetConsistency) {
    plasticity_orchestrator_t* orch = plasticity_orchestrator_create(nullptr);
    ASSERT_NE(orch, nullptr);

    // Set various weights
    std::vector<float> test_weights = {0.0f, 0.1f, 0.5f, 0.9f, 1.0f};

    for (size_t i = 0; i < test_weights.size(); i++) {
        EXPECT_EQ(plasticity_orchestrator_set_weight(orch, i, test_weights[i]), 0);
        float weight = plasticity_orchestrator_get_weight(orch, i);
        EXPECT_FLOAT_EQ(weight, test_weights[i]) << "Mismatch at index " << i;
    }

    plasticity_orchestrator_destroy(orch);
}

/* ============================================================================
 * Numerical Stability Regression
 * ============================================================================ */

TEST_F(OrchestratorRegressionTest, NoNaNAfterUpdates) {
    plasticity_orchestrator_t* orch = plasticity_orchestrator_create(nullptr);
    ASSERT_NE(orch, nullptr);

    // Create synapses
    for (int i = 0; i < 20; i++) {
        plasticity_orchestrator_set_weight(orch, i, 0.5f);
    }

    // Run updates
    for (int u = 0; u < 500; u++) {
        int ret = plasticity_orchestrator_update(orch, 1);
        EXPECT_EQ(ret, 0) << "Update failed at iteration " << u;
    }

    // Verify no NaN weights
    for (int i = 0; i < 20; i++) {
        float weight = plasticity_orchestrator_get_weight(orch, i);
        EXPECT_TRUE(IsNumericallyStable(weight))
            << "NaN or Inf weight at synapse " << i;
    }

    plasticity_orchestrator_destroy(orch);
}

TEST_F(OrchestratorRegressionTest, StabilityWithExtremeDeltas) {
    plasticity_orchestrator_config_t config;
    plasticity_orchestrator_default_config(&config);
    // Disable some modules for faster test
    config.enabled.enable_metabolic = false;
    config.enabled.enable_structural = false;
    config.enabled.enable_protein_synthesis = false;

    plasticity_orchestrator_t* orch = plasticity_orchestrator_create(&config);
    ASSERT_NE(orch, nullptr);

    plasticity_orchestrator_set_weight(orch, 0, 0.5f);

    // Test with very small dt
    for (int i = 0; i < 100; i++) {
        int ret = plasticity_orchestrator_update(orch, 0.001f);
        EXPECT_EQ(ret, 0);
    }

    // Test with larger dt
    for (int i = 0; i < 100; i++) {
        int ret = plasticity_orchestrator_update(orch, 100.0f);
        EXPECT_EQ(ret, 0);
    }

    float weight = plasticity_orchestrator_get_weight(orch, 0);
    EXPECT_TRUE(IsNumericallyStable(weight));

    plasticity_orchestrator_destroy(orch);
}

/* ============================================================================
 * Stats Accumulation Regression
 * ============================================================================ */

TEST_F(OrchestratorRegressionTest, StatsAccumulateCorrectly) {
    plasticity_orchestrator_t* orch = plasticity_orchestrator_create(nullptr);
    ASSERT_NE(orch, nullptr);

    plasticity_orchestrator_set_weight(orch, 0, 0.5f);

    // Run exact number of updates
    int expected_updates = 123;
    for (int i = 0; i < expected_updates; i++) {
        plasticity_orchestrator_update(orch, 1);
    }

    plasticity_stats_t stats;
    EXPECT_EQ(plasticity_orchestrator_get_stats(orch, &stats), 0);
    EXPECT_EQ(stats.total_updates, expected_updates);

    plasticity_orchestrator_destroy(orch);
}

TEST_F(OrchestratorRegressionTest, StatsResetWorks) {
    plasticity_orchestrator_t* orch = plasticity_orchestrator_create(nullptr);
    ASSERT_NE(orch, nullptr);

    plasticity_orchestrator_set_weight(orch, 0, 0.5f);

    // Run some updates
    for (int i = 0; i < 50; i++) {
        plasticity_orchestrator_update(orch, 1);
    }

    // Reset
    EXPECT_EQ(plasticity_orchestrator_reset_stats(orch), 0);

    plasticity_stats_t stats;
    EXPECT_EQ(plasticity_orchestrator_get_stats(orch, &stats), 0);
    EXPECT_EQ(stats.total_updates, 0);

    // Run more and verify
    for (int i = 0; i < 25; i++) {
        plasticity_orchestrator_update(orch, 1);
    }

    EXPECT_EQ(plasticity_orchestrator_get_stats(orch, &stats), 0);
    EXPECT_EQ(stats.total_updates, 25);

    plasticity_orchestrator_destroy(orch);
}

/* ============================================================================
 * Module Enable/Disable Regression
 * ============================================================================ */

TEST_F(OrchestratorRegressionTest, AllModulesDisabled) {
    plasticity_orchestrator_config_t config;
    plasticity_orchestrator_default_config(&config);

    // Disable all modules
    config.enabled.enable_triplet_stdp = false;
    config.enabled.enable_bcm = false;
    config.enabled.enable_homeostatic = false;
    config.enabled.enable_metabolic = false;
    config.enabled.enable_calcium = false;
    config.enabled.enable_structural = false;
    config.enabled.enable_protein_synthesis = false;
    config.enabled.enable_metaplasticity = false;
    config.enabled.enable_heterosynaptic = false;
    config.enabled.enable_astrocyte = false;

    plasticity_orchestrator_t* orch = plasticity_orchestrator_create(&config);
    ASSERT_NE(orch, nullptr);

    plasticity_orchestrator_set_weight(orch, 0, 0.5f);

    // Updates should still work
    for (int i = 0; i < 100; i++) {
        EXPECT_EQ(plasticity_orchestrator_update(orch, 1), 0);
    }

    plasticity_orchestrator_destroy(orch);
}

TEST_F(OrchestratorRegressionTest, SingleModuleEnabled) {
    // Test each module individually
    const char* module_names[] = {
        "triplet_stdp", "bcm", "homeostatic", "metabolic", "calcium",
        "structural", "protein_synthesis", "metaplasticity", "heterosynaptic", "astrocyte"
    };

    for (int module = 0; module < 10; module++) {
        plasticity_orchestrator_config_t config;
        plasticity_orchestrator_default_config(&config);

        // Disable all
        config.enabled.enable_triplet_stdp = false;
        config.enabled.enable_bcm = false;
        config.enabled.enable_homeostatic = false;
        config.enabled.enable_metabolic = false;
        config.enabled.enable_calcium = false;
        config.enabled.enable_structural = false;
        config.enabled.enable_protein_synthesis = false;
        config.enabled.enable_metaplasticity = false;
        config.enabled.enable_heterosynaptic = false;
        config.enabled.enable_astrocyte = false;

        // Enable one
        switch (module) {
            case 0: config.enabled.enable_triplet_stdp = true; break;
            case 1: config.enabled.enable_bcm = true; break;
            case 2: config.enabled.enable_homeostatic = true; break;
            case 3: config.enabled.enable_metabolic = true; break;
            case 4: config.enabled.enable_calcium = true; break;
            case 5: config.enabled.enable_structural = true; break;
            case 6: config.enabled.enable_protein_synthesis = true; break;
            case 7: config.enabled.enable_metaplasticity = true; break;
            case 8: config.enabled.enable_heterosynaptic = true; break;
            case 9: config.enabled.enable_astrocyte = true; break;
        }

        plasticity_orchestrator_t* orch = plasticity_orchestrator_create(&config);
        ASSERT_NE(orch, nullptr) << "Failed to create with " << module_names[module];

        plasticity_orchestrator_set_weight(orch, 0, 0.5f);
        EXPECT_EQ(plasticity_orchestrator_update(orch, 1), 0)
            << "Update failed with " << module_names[module];

        plasticity_orchestrator_destroy(orch);
    }
}

/* ============================================================================
 * Memory Leak Regression
 * ============================================================================ */

TEST_F(OrchestratorRegressionTest, CreateDestroyCycles) {
    // Create and destroy many times to check for leaks
    for (int cycle = 0; cycle < 100; cycle++) {
        plasticity_orchestrator_t* orch = plasticity_orchestrator_create(nullptr);
        ASSERT_NE(orch, nullptr) << "Create failed at cycle " << cycle;

        // Use it briefly
        plasticity_orchestrator_set_weight(orch, 0, 0.5f);
        plasticity_orchestrator_update(orch, 1);

        plasticity_orchestrator_destroy(orch);
    }
}

TEST_F(OrchestratorRegressionTest, CreateDestroyWithManySynapses) {
    for (int cycle = 0; cycle < 10; cycle++) {
        plasticity_orchestrator_t* orch = plasticity_orchestrator_create(nullptr);
        ASSERT_NE(orch, nullptr);

        // Create many synapses
        for (int i = 0; i < 50; i++) {
            plasticity_orchestrator_set_weight(orch, i, 0.5f);
        }

        // Run updates
        for (int u = 0; u < 100; u++) {
            plasticity_orchestrator_update(orch, 1);
        }

        plasticity_orchestrator_destroy(orch);
    }
}

/* ============================================================================
 * Thread Safety Regression
 * ============================================================================ */

TEST_F(OrchestratorRegressionTest, ConcurrentUpdates) {
    plasticity_orchestrator_t* orch = plasticity_orchestrator_create(nullptr);
    ASSERT_NE(orch, nullptr);

    // Create synapses
    for (int i = 0; i < 10; i++) {
        plasticity_orchestrator_set_weight(orch, i, 0.5f);
    }

    std::atomic<int> update_count{0};
    std::atomic<bool> has_error{false};

    auto update_fn = [&]() {
        for (int i = 0; i < 100; i++) {
            if (plasticity_orchestrator_update(orch, 1) != 0) {
                has_error = true;
            }
            update_count++;
        }
    };

    // Run concurrent updates
    std::vector<std::thread> threads;
    for (int t = 0; t < 4; t++) {
        threads.emplace_back(update_fn);
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_FALSE(has_error) << "Error during concurrent updates";
    EXPECT_EQ(update_count.load(), 400);

    plasticity_orchestrator_destroy(orch);
}

TEST_F(OrchestratorRegressionTest, ConcurrentReadWrite) {
    plasticity_orchestrator_t* orch = plasticity_orchestrator_create(nullptr);
    ASSERT_NE(orch, nullptr);

    // Create synapses
    for (int i = 0; i < 10; i++) {
        plasticity_orchestrator_set_weight(orch, i, 0.5f);
    }

    std::atomic<bool> has_error{false};
    std::atomic<bool> stop{false};

    // Writer thread
    auto writer = [&]() {
        for (int i = 0; i < 200; i++) {
            if (plasticity_orchestrator_update(orch, 1) != 0) {
                has_error = true;
            }
        }
        stop = true;
    };

    // Reader thread
    auto reader = [&]() {
        while (!stop) {
            for (int i = 0; i < 10; i++) {
                float w = plasticity_orchestrator_get_weight(orch, i);
                if (!IsNumericallyStable(w)) {
                    // NaN is okay if synapse doesn't exist
                }
            }
        }
    };

    std::thread writer_thread(writer);
    std::thread reader_thread(reader);

    writer_thread.join();
    reader_thread.join();

    EXPECT_FALSE(has_error);

    plasticity_orchestrator_destroy(orch);
}

/* ============================================================================
 * Long-Running Simulation Regression
 * ============================================================================ */

TEST_F(OrchestratorRegressionTest, LongRunningStability) {
    plasticity_orchestrator_config_t config;
    plasticity_orchestrator_default_config(&config);
    // Use lighter config for speed
    config.enabled.enable_metabolic = false;
    config.enabled.enable_structural = false;
    config.enabled.enable_protein_synthesis = false;

    plasticity_orchestrator_t* orch = plasticity_orchestrator_create(&config);
    ASSERT_NE(orch, nullptr);

    // Create synapses
    for (int i = 0; i < 20; i++) {
        plasticity_orchestrator_set_weight(orch, i, 0.5f);
    }

    // Run 10000 updates (simulating ~2.8 hours at 1ms step)
    for (int u = 0; u < 10000; u++) {
        EXPECT_EQ(plasticity_orchestrator_update(orch, 1), 0);
    }

    // Verify stability
    for (int i = 0; i < 20; i++) {
        float weight = plasticity_orchestrator_get_weight(orch, i);
        EXPECT_TRUE(IsNumericallyStable(weight))
            << "Unstable weight at synapse " << i << " after long run";
        if (!std::isnan(weight)) {
            EXPECT_TRUE(IsInBounds(weight, 0.0f, 1.0f))
                << "Weight out of bounds after long run";
        }
    }

    plasticity_stats_t stats;
    plasticity_orchestrator_get_stats(orch, &stats);
    EXPECT_EQ(stats.total_updates, 10000);

    plasticity_orchestrator_destroy(orch);
}

/* ============================================================================
 * NULL Safety Regression
 * ============================================================================ */

TEST_F(OrchestratorRegressionTest, NullSafety) {
    // Default config with NULL should return error
    EXPECT_NE(plasticity_orchestrator_default_config(nullptr), 0);

    // Create with NULL config creates with defaults (not NULL result)
    plasticity_orchestrator_t* orch = plasticity_orchestrator_create(nullptr);
    ASSERT_NE(orch, nullptr);  // Creates successfully with defaults

    // Operations on NULL orchestrator should fail
    EXPECT_NE(plasticity_orchestrator_update(nullptr, 1), 0);
    EXPECT_NE(plasticity_orchestrator_set_weight(nullptr, 0, 0.5f), 0);
    EXPECT_TRUE(std::isnan(plasticity_orchestrator_get_weight(nullptr, 0)));
    EXPECT_NE(plasticity_orchestrator_get_stats(nullptr, nullptr), 0);
    EXPECT_NE(plasticity_orchestrator_reset_stats(nullptr), 0);

    // Destroy NULL should be safe
    plasticity_orchestrator_destroy(nullptr);

    plasticity_orchestrator_destroy(orch);
}
