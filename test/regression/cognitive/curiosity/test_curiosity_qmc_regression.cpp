/**
 * @file test_curiosity_qmc_regression.cpp
 * @brief Regression tests for Curiosity QMC (Step 10 MC Integration)
 * @version 1.0.0
 * @date 2026-01-12
 *
 * WHAT: Regression tests for QMC-based uncertainty and empowerment in curiosity
 * WHY:  Ensure stability, determinism, performance, memory safety across updates
 * HOW:  Stress tests, bounds checking, determinism verification, memory checks
 *
 * @author NIMCP Development Team
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <vector>
#include <chrono>
#include <random>

extern "C" {
#include "cognitive/curiosity/nimcp_curiosity_enhanced.h"
#include "cognitive/curiosity/nimcp_curiosity.h"
#include "core/brain/nimcp_brain.h"
#include "utils/memory/nimcp_memory.h"
}

/* ============================================================================
 * Test Fixtures
 * ============================================================================ */

class CuriosityQMCRegressionTest : public ::testing::Test {
protected:
    curiosity_enhanced_system_t* system = nullptr;
    curiosity_enhanced_config_t config;
    brain_t brain = nullptr;
    curiosity_engine_t base_engine = nullptr;

    void SetUp() override {
        /* Create brain for base engine */
        brain = brain_create("qmc_regression_test", BRAIN_SIZE_TINY,
                             BRAIN_TASK_CLASSIFICATION, 10, 5);
        ASSERT_NE(brain, nullptr);

        /* Create base curiosity engine */
        base_engine = curiosity_engine_create(brain, "regression_learner");
        ASSERT_NE(base_engine, nullptr);

        /* Create enhanced system */
        curiosity_enhanced_config_default(&config);
        config.enable_all_enhancements = true;
        config.enable_quantum_curiosity = true;
        system = curiosity_enhanced_create(&config, base_engine);
        ASSERT_NE(system, nullptr);
    }

    void TearDown() override {
        if (system) {
            curiosity_enhanced_destroy(system);
            system = nullptr;
        }
        if (base_engine) {
            curiosity_engine_destroy(base_engine);
            base_engine = nullptr;
        }
        if (brain) {
            brain_destroy(brain);
            brain = nullptr;
        }
    }
};

class CuriosityBaseQMCRegressionTest : public ::testing::Test {
protected:
    brain_t brain = nullptr;
    curiosity_engine_t engine = nullptr;

    void SetUp() override {
        brain = brain_create("base_qmc_regression", BRAIN_SIZE_TINY,
                             BRAIN_TASK_CLASSIFICATION, 10, 5);
        ASSERT_NE(brain, nullptr);

        engine = curiosity_engine_create(brain, "regression_learner");
        ASSERT_NE(engine, nullptr);
    }

    void TearDown() override {
        if (engine) {
            curiosity_engine_destroy(engine);
            engine = nullptr;
        }
        if (brain) {
            brain_destroy(brain);
            brain = nullptr;
        }
    }
};

/* ============================================================================
 * Determinism Tests
 * ============================================================================ */

TEST_F(CuriosityQMCRegressionTest, UncertaintyDeterministicWithSameSeed) {
    const uint32_t test_seed = 12345;

    // First run
    curiosity_qmc_config_t qmc_config;
    curiosity_enhanced_qmc_default_config(&qmc_config);
    qmc_config.seed = test_seed;
    curiosity_enhanced_set_qmc_config(system, &qmc_config);

    curiosity_qmc_uncertainty_t result1;
    curiosity_enhanced_estimate_uncertainty(system, "determinism_topic", &result1);

    // Reset seed
    qmc_config.seed = test_seed;
    curiosity_enhanced_set_qmc_config(system, &qmc_config);

    curiosity_qmc_uncertainty_t result2;
    curiosity_enhanced_estimate_uncertainty(system, "determinism_topic", &result2);

    // Results should be identical
    EXPECT_FLOAT_EQ(result1.mean_interest, result2.mean_interest);
    EXPECT_FLOAT_EQ(result1.variance, result2.variance);
    EXPECT_FLOAT_EQ(result1.std_error, result2.std_error);
    EXPECT_FLOAT_EQ(result1.epistemic_uncertainty, result2.epistemic_uncertainty);
}

TEST_F(CuriosityQMCRegressionTest, EmpowermentDeterministicWithSameSeed) {
    const uint32_t test_seed = 54321;

    // First run
    curiosity_qmc_config_t qmc_config;
    curiosity_enhanced_qmc_default_config(&qmc_config);
    qmc_config.seed = test_seed;
    curiosity_enhanced_set_qmc_config(system, &qmc_config);

    curiosity_empowerment_result_t result1;
    curiosity_enhanced_compute_empowerment(system, "emp_topic", 3, &result1);

    // Reset seed
    qmc_config.seed = test_seed;
    curiosity_enhanced_set_qmc_config(system, &qmc_config);

    curiosity_empowerment_result_t result2;
    curiosity_enhanced_compute_empowerment(system, "emp_topic", 3, &result2);

    // Results should be identical
    EXPECT_FLOAT_EQ(result1.empowerment, result2.empowerment);
    EXPECT_FLOAT_EQ(result1.empowerment_normalized, result2.empowerment_normalized);
    EXPECT_EQ(result1.num_actions, result2.num_actions);

    curiosity_empowerment_result_free(&result1);
    curiosity_empowerment_result_free(&result2);
}

/* ============================================================================
 * Bounds Stability Tests
 * ============================================================================ */

TEST_F(CuriosityQMCRegressionTest, UncertaintyBoundsStability) {
    // Run many uncertainty estimates and verify bounds
    for (int i = 0; i < 100; i++) {
        char topic[32];
        snprintf(topic, sizeof(topic), "bounds_topic_%d", i);

        curiosity_qmc_uncertainty_t result;
        int ret = curiosity_enhanced_estimate_uncertainty(system, topic, &result);
        ASSERT_EQ(ret, 0);

        // All values should be within expected bounds
        EXPECT_GE(result.mean_interest, 0.0f) << "mean_interest below 0 at iteration " << i;
        EXPECT_LE(result.mean_interest, 1.0f) << "mean_interest above 1 at iteration " << i;
        EXPECT_GE(result.variance, 0.0f) << "variance negative at iteration " << i;
        EXPECT_GE(result.std_error, 0.0f) << "std_error negative at iteration " << i;
        EXPECT_GE(result.confidence_95_lower, 0.0f) << "CI lower below 0 at iteration " << i;
        EXPECT_LE(result.confidence_95_upper, 1.0f) << "CI upper above 1 at iteration " << i;
        EXPECT_LE(result.confidence_95_lower, result.confidence_95_upper) << "CI inverted at iteration " << i;
        EXPECT_GE(result.epistemic_uncertainty, 0.0f) << "epistemic uncertainty negative at iteration " << i;
        EXPECT_GE(result.aleatoric_uncertainty, 0.0f) << "aleatoric uncertainty negative at iteration " << i;
    }
}

TEST_F(CuriosityQMCRegressionTest, EmpowermentBoundsStability) {
    for (int i = 0; i < 50; i++) {
        char topic[32];
        snprintf(topic, sizeof(topic), "emp_bounds_%d", i);

        curiosity_empowerment_result_t result;
        int ret = curiosity_enhanced_compute_empowerment(system, topic, 3, &result);
        ASSERT_EQ(ret, 0);

        EXPECT_GE(result.empowerment, 0.0f) << "empowerment negative at iteration " << i;
        EXPECT_GE(result.empowerment_normalized, 0.0f) << "normalized empowerment negative at iteration " << i;
        EXPECT_LE(result.empowerment_normalized, 1.0f) << "normalized empowerment above 1 at iteration " << i;
        EXPECT_GT(result.num_actions, 0u) << "num_actions zero at iteration " << i;
        EXPECT_GE(result.entropy_current, 0.0f) << "entropy negative at iteration " << i;
        EXPECT_GE(result.channel_capacity, 0.0f) << "channel capacity negative at iteration " << i;

        // Empowerment should not exceed channel capacity
        EXPECT_LE(result.empowerment, result.channel_capacity + 0.01f)
            << "empowerment exceeds channel capacity at iteration " << i;

        curiosity_empowerment_result_free(&result);
    }
}

TEST_F(CuriosityQMCRegressionTest, ExplorationBonusBoundsStability) {
    curiosity_qmc_config_t qmc_config;
    curiosity_enhanced_qmc_default_config(&qmc_config);
    curiosity_enhanced_set_qmc_config(system, &qmc_config);

    for (int i = 0; i < 100; i++) {
        char topic[32];
        snprintf(topic, sizeof(topic), "bonus_topic_%d", i);

        float bonus = curiosity_enhanced_get_exploration_bonus(system, topic);

        EXPECT_GE(bonus, 0.0f) << "bonus negative at iteration " << i;
        EXPECT_LE(bonus, qmc_config.exploration_bonus + 0.01f)
            << "bonus exceeds max at iteration " << i;
    }
}

TEST_F(CuriosityQMCRegressionTest, InfoGainBoundsStability) {
    for (int i = 0; i < 100; i++) {
        char topic[32];
        snprintf(topic, sizeof(topic), "info_topic_%d", i);

        float info_gain = curiosity_enhanced_estimate_info_gain_qmc(system, topic);

        EXPECT_GE(info_gain, 0.0f) << "info gain negative at iteration " << i;
        EXPECT_LE(info_gain, 5.0f) << "info gain exceeds cap at iteration " << i;
    }
}

/* ============================================================================
 * Performance Regression Tests
 * ============================================================================ */

TEST_F(CuriosityQMCRegressionTest, UncertaintyEstimationPerformance) {
    const int num_iterations = 100;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < num_iterations; i++) {
        curiosity_qmc_uncertainty_t result;
        curiosity_enhanced_estimate_uncertainty(system, "perf_topic", &result);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Should complete 100 iterations in reasonable time (< 5 seconds)
    EXPECT_LT(duration.count(), 5000) << "Uncertainty estimation too slow: " << duration.count() << "ms";

    // Average per-iteration should be < 50ms
    float avg_ms = static_cast<float>(duration.count()) / num_iterations;
    EXPECT_LT(avg_ms, 50.0f) << "Average iteration time: " << avg_ms << "ms";
}

TEST_F(CuriosityQMCRegressionTest, EmpowermentComputationPerformance) {
    const int num_iterations = 50;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < num_iterations; i++) {
        curiosity_empowerment_result_t result;
        curiosity_enhanced_compute_empowerment(system, "perf_topic", 3, &result);
        curiosity_empowerment_result_free(&result);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Should complete 50 iterations in reasonable time (< 10 seconds)
    EXPECT_LT(duration.count(), 10000) << "Empowerment computation too slow: " << duration.count() << "ms";
}

TEST_F(CuriosityQMCRegressionTest, BatchUncertaintyPerformance) {
    const char* topics[] = {
        "batch_topic_1", "batch_topic_2", "batch_topic_3", "batch_topic_4",
        "batch_topic_5", "batch_topic_6", "batch_topic_7", "batch_topic_8"
    };
    curiosity_qmc_uncertainty_t results[8];

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 20; i++) {
        curiosity_enhanced_estimate_uncertainty_batch(system, topics, 8, results);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Batch should be reasonably fast
    EXPECT_LT(duration.count(), 10000) << "Batch uncertainty too slow: " << duration.count() << "ms";
}

/* ============================================================================
 * Memory Safety Tests
 * ============================================================================ */

TEST_F(CuriosityQMCRegressionTest, EmpowermentMemoryLeakCheck) {
    // Repeatedly allocate and free empowerment results
    for (int i = 0; i < 1000; i++) {
        curiosity_empowerment_result_t result;
        int ret = curiosity_enhanced_compute_empowerment(system, "memory_topic", 3, &result);
        ASSERT_EQ(ret, 0);
        ASSERT_NE(result.action_empowerment, nullptr);

        curiosity_empowerment_result_free(&result);
        EXPECT_EQ(result.action_empowerment, nullptr);
    }
}

TEST_F(CuriosityQMCRegressionTest, DoubleFreeProtection) {
    curiosity_empowerment_result_t result;
    curiosity_enhanced_compute_empowerment(system, "double_free_topic", 3, &result);

    curiosity_empowerment_result_free(&result);
    curiosity_empowerment_result_free(&result);  // Should be safe (NULL check)
}

TEST_F(CuriosityQMCRegressionTest, NullFreeProtection) {
    curiosity_empowerment_result_free(nullptr);  // Should not crash
}

/* ============================================================================
 * Stress Tests
 * ============================================================================ */

TEST_F(CuriosityQMCRegressionTest, HighVolumeUncertaintyStress) {
    curiosity_enhanced_reset_qmc_stats(system);

    const int num_operations = 500;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> topic_dist(0, 99);

    for (int i = 0; i < num_operations; i++) {
        char topic[32];
        snprintf(topic, sizeof(topic), "stress_topic_%d", topic_dist(gen));

        curiosity_qmc_uncertainty_t result;
        int ret = curiosity_enhanced_estimate_uncertainty(system, topic, &result);
        ASSERT_EQ(ret, 0);
    }

    curiosity_qmc_stats_t stats;
    curiosity_enhanced_get_qmc_stats(system, &stats);
    EXPECT_EQ(stats.uncertainty_estimations, static_cast<uint64_t>(num_operations));
}

TEST_F(CuriosityQMCRegressionTest, MixedOperationsStress) {
    curiosity_enhanced_reset_qmc_stats(system);

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> op_dist(0, 4);

    for (int i = 0; i < 200; i++) {
        char topic[32];
        snprintf(topic, sizeof(topic), "mixed_topic_%d", i % 20);

        switch (op_dist(gen)) {
            case 0: {
                curiosity_qmc_uncertainty_t uncertainty;
                curiosity_enhanced_estimate_uncertainty(system, topic, &uncertainty);
                break;
            }
            case 1: {
                curiosity_empowerment_result_t emp;
                curiosity_enhanced_compute_empowerment(system, topic, 3, &emp);
                curiosity_empowerment_result_free(&emp);
                break;
            }
            case 2: {
                curiosity_enhanced_get_exploration_bonus(system, topic);
                break;
            }
            case 3: {
                curiosity_enhanced_estimate_info_gain_qmc(system, topic);
                break;
            }
            case 4: {
                curiosity_enhanced_update_interest_mc(system, topic, 0.5f, 0.1f);
                break;
            }
        }
    }
}

/* ============================================================================
 * Base Curiosity Regression Tests
 * ============================================================================ */

TEST_F(CuriosityBaseQMCRegressionTest, EmpowermentBoundsStability) {
    for (int i = 0; i < 50; i++) {
        char concept_name[32];
        snprintf(concept_name, sizeof(concept_name), "base_emp_%d", i);

        curiosity_empowerment_t result;
        int ret = curiosity_compute_empowerment(engine, concept_name, 3, &result);
        ASSERT_EQ(ret, 0);

        EXPECT_GE(result.empowerment, 0.0f);
        EXPECT_GE(result.empowerment_normalized, 0.0f);
        EXPECT_LE(result.empowerment_normalized, 1.0f);
        EXPECT_GT(result.action_count, 0u);
    }
}

TEST_F(CuriosityBaseQMCRegressionTest, IntrinsicRewardBoundsStability) {
    for (int i = 0; i < 100; i++) {
        char concept_name[32];
        snprintf(concept_name, sizeof(concept_name), "reward_%d", i);

        float reward = curiosity_compute_intrinsic_reward(engine, concept_name, 0.5f, 0.5f);

        EXPECT_GE(reward, 0.0f) << "reward negative at iteration " << i;
        EXPECT_LE(reward, 1.0f) << "reward above 1 at iteration " << i;
    }
}

TEST_F(CuriosityBaseQMCRegressionTest, SamplingDistributionStability) {
    const char* concepts[] = {"c1", "c2", "c3", "c4"};

    for (int i = 0; i < 100; i++) {
        uint32_t selected = curiosity_sample_by_empowerment(engine, concepts, 4, 1.0f);
        EXPECT_LT(selected, 4u) << "selection out of bounds at iteration " << i;
    }
}

TEST_F(CuriosityBaseQMCRegressionTest, EmpowermentChangeStability) {
    for (int i = 0; i < 50; i++) {
        char concept_name[32];
        snprintf(concept_name, sizeof(concept_name), "change_%d", i);

        float change = curiosity_estimate_empowerment_change(engine, concept_name, 100);

        // Change should be bounded and reasonable
        EXPECT_GT(change, -2.0f) << "change too negative at iteration " << i;
        EXPECT_LT(change, 2.0f) << "change too positive at iteration " << i;
    }
}

/* ============================================================================
 * Statistics Consistency Tests
 * ============================================================================ */

TEST_F(CuriosityQMCRegressionTest, StatsConsistency) {
    curiosity_enhanced_reset_qmc_stats(system);

    // Perform known number of operations
    const uint32_t expected_uncertainty = 10;
    const uint32_t expected_empowerment = 5;

    for (uint32_t i = 0; i < expected_uncertainty; i++) {
        curiosity_qmc_uncertainty_t result;
        curiosity_enhanced_estimate_uncertainty(system, "stats_topic", &result);
    }

    for (uint32_t i = 0; i < expected_empowerment; i++) {
        curiosity_empowerment_result_t emp;
        curiosity_enhanced_compute_empowerment(system, "stats_topic", 3, &emp);
        curiosity_empowerment_result_free(&emp);
    }

    curiosity_qmc_stats_t stats;
    curiosity_enhanced_get_qmc_stats(system, &stats);

    EXPECT_EQ(stats.uncertainty_estimations, expected_uncertainty);
    EXPECT_EQ(stats.empowerment_calculations, expected_empowerment);
    EXPECT_GT(stats.mc_samples_total, 0u);
}

TEST_F(CuriosityQMCRegressionTest, StatsResetConsistency) {
    // Generate some stats
    curiosity_qmc_uncertainty_t result;
    curiosity_enhanced_estimate_uncertainty(system, "topic", &result);

    curiosity_qmc_stats_t before_reset;
    curiosity_enhanced_get_qmc_stats(system, &before_reset);
    EXPECT_GT(before_reset.uncertainty_estimations, 0u);

    // Reset
    curiosity_enhanced_reset_qmc_stats(system);

    // Verify reset
    curiosity_qmc_stats_t after_reset;
    curiosity_enhanced_get_qmc_stats(system, &after_reset);

    EXPECT_EQ(after_reset.uncertainty_estimations, 0u);
    EXPECT_EQ(after_reset.empowerment_calculations, 0u);
    EXPECT_EQ(after_reset.mc_samples_total, 0u);
    EXPECT_EQ(after_reset.high_uncertainty_topics, 0u);
    EXPECT_FLOAT_EQ(after_reset.avg_epistemic_uncertainty, 0.0f);
    EXPECT_FLOAT_EQ(after_reset.avg_empowerment, 0.0f);
    EXPECT_FLOAT_EQ(after_reset.total_exploration_bonus, 0.0f);
}
