/**
 * @file test_curiosity_qmc.cpp
 * @brief Unit tests for Curiosity QMC (Quantum Monte Carlo) Integration
 * @version 1.0.0
 * @date 2026-01-12
 *
 * WHAT: Tests for QMC-based uncertainty estimation and empowerment in curiosity
 * WHY:  Verify Step 10 of MC Integration Plan - Curiosity MCS
 * HOW:  Test uncertainty estimation, empowerment, exploration bonus, info gain
 *
 * @author NIMCP Development Team
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <vector>
#include <string>

extern "C" {
#include "cognitive/curiosity/nimcp_curiosity_enhanced.h"
#include "cognitive/curiosity/nimcp_curiosity.h"
#include "core/brain/nimcp_brain.h"
#include "utils/memory/nimcp_memory.h"
}

/* ============================================================================
 * Test Fixtures
 * ============================================================================ */

class CuriosityQMCTest : public ::testing::Test {
protected:
    curiosity_enhanced_system_t* system = nullptr;
    curiosity_enhanced_config_t config;
    brain_t brain = nullptr;
    curiosity_engine_t base_engine = nullptr;

    void SetUp() override {
        /* Create brain for base engine */
        brain = brain_create("curiosity_qmc_test", BRAIN_SIZE_TINY,
                             BRAIN_TASK_CLASSIFICATION, 10, 5);
        ASSERT_NE(brain, nullptr);

        /* Create base curiosity engine */
        base_engine = curiosity_engine_create(brain, "test_learner");
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

class CuriosityBaseQMCTest : public ::testing::Test {
protected:
    brain_t brain = nullptr;
    curiosity_engine_t engine = nullptr;

    void SetUp() override {
        brain = brain_create("base_qmc_test", BRAIN_SIZE_TINY,
                             BRAIN_TASK_CLASSIFICATION, 10, 5);
        ASSERT_NE(brain, nullptr);

        engine = curiosity_engine_create(brain, "test_learner");
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
 * Enhanced Curiosity QMC Tests
 * ============================================================================ */

TEST_F(CuriosityQMCTest, DefaultConfigInitialization) {
    curiosity_qmc_config_t qmc_config;
    curiosity_enhanced_qmc_default_config(&qmc_config);

    EXPECT_EQ(qmc_config.num_samples, 1000u);
    EXPECT_EQ(qmc_config.burnin, 100u);
    EXPECT_FLOAT_EQ(qmc_config.uncertainty_threshold, 0.3f);
    EXPECT_FLOAT_EQ(qmc_config.exploration_bonus, 0.2f);
    EXPECT_TRUE(qmc_config.enable_empowerment);
    EXPECT_EQ(qmc_config.empowerment_horizon, 3u);
    EXPECT_FLOAT_EQ(qmc_config.temperature, 1.0f);
}

TEST_F(CuriosityQMCTest, SetQMCConfig) {
    curiosity_qmc_config_t qmc_config;
    curiosity_enhanced_qmc_default_config(&qmc_config);
    qmc_config.num_samples = 500;
    qmc_config.uncertainty_threshold = 0.5f;
    qmc_config.seed = 12345;

    int ret = curiosity_enhanced_set_qmc_config(system, &qmc_config);
    EXPECT_EQ(ret, 0);
}

TEST_F(CuriosityQMCTest, SetQMCConfigNullSystem) {
    curiosity_qmc_config_t qmc_config;
    curiosity_enhanced_qmc_default_config(&qmc_config);

    int ret = curiosity_enhanced_set_qmc_config(nullptr, &qmc_config);
    EXPECT_NE(ret, 0);
}

TEST_F(CuriosityQMCTest, SetQMCConfigNullConfig) {
    int ret = curiosity_enhanced_set_qmc_config(system, nullptr);
    EXPECT_NE(ret, 0);
}

TEST_F(CuriosityQMCTest, EstimateUncertaintyBasic) {
    curiosity_qmc_uncertainty_t result;
    int ret = curiosity_enhanced_estimate_uncertainty(system, "test_topic", &result);

    EXPECT_EQ(ret, 0);
    EXPECT_GE(result.mean_interest, 0.0f);
    EXPECT_LE(result.mean_interest, 1.0f);
    EXPECT_GE(result.variance, 0.0f);
    EXPECT_GE(result.std_error, 0.0f);
    EXPECT_LE(result.confidence_95_lower, result.confidence_95_upper);
    EXPECT_GE(result.epistemic_uncertainty, 0.0f);
    EXPECT_GE(result.aleatoric_uncertainty, 0.0f);
    EXPECT_GT(result.effective_samples, 0u);
}

TEST_F(CuriosityQMCTest, EstimateUncertaintyNullParams) {
    curiosity_qmc_uncertainty_t result;

    EXPECT_NE(curiosity_enhanced_estimate_uncertainty(nullptr, "topic", &result), 0);
    EXPECT_NE(curiosity_enhanced_estimate_uncertainty(system, nullptr, &result), 0);
    EXPECT_NE(curiosity_enhanced_estimate_uncertainty(system, "topic", nullptr), 0);
}

TEST_F(CuriosityQMCTest, EstimateUncertaintyBatch) {
    const char* topics[] = {"topic1", "topic2", "topic3"};
    curiosity_qmc_uncertainty_t results[3];

    int ret = curiosity_enhanced_estimate_uncertainty_batch(system, topics, 3, results);
    EXPECT_EQ(ret, 0);

    for (int i = 0; i < 3; i++) {
        EXPECT_GE(results[i].mean_interest, 0.0f);
        EXPECT_LE(results[i].mean_interest, 1.0f);
    }
}

TEST_F(CuriosityQMCTest, ComputeEmpowermentBasic) {
    curiosity_empowerment_result_t result;
    int ret = curiosity_enhanced_compute_empowerment(system, "test_topic", 3, &result);

    EXPECT_EQ(ret, 0);
    EXPECT_GE(result.empowerment, 0.0f);
    EXPECT_GE(result.empowerment_normalized, 0.0f);
    EXPECT_LE(result.empowerment_normalized, 1.0f);
    EXPECT_GT(result.num_actions, 0u);
    EXPECT_GE(result.entropy_current, 0.0f);
    EXPECT_GE(result.channel_capacity, 0.0f);

    curiosity_empowerment_result_free(&result);
}

TEST_F(CuriosityQMCTest, ComputeEmpowermentDefaultHorizon) {
    curiosity_empowerment_result_t result;
    int ret = curiosity_enhanced_compute_empowerment(system, "test_topic", 0, &result);

    EXPECT_EQ(ret, 0);
    EXPECT_GE(result.empowerment, 0.0f);

    curiosity_empowerment_result_free(&result);
}

TEST_F(CuriosityQMCTest, ComputeEmpowermentNullParams) {
    curiosity_empowerment_result_t result;

    EXPECT_NE(curiosity_enhanced_compute_empowerment(nullptr, "topic", 3, &result), 0);
    EXPECT_NE(curiosity_enhanced_compute_empowerment(system, nullptr, 3, &result), 0);
    EXPECT_NE(curiosity_enhanced_compute_empowerment(system, "topic", 3, nullptr), 0);
}

TEST_F(CuriosityQMCTest, EmpowermentResultFreeNullSafe) {
    curiosity_empowerment_result_free(nullptr);  // Should not crash
}

TEST_F(CuriosityQMCTest, SampleByUncertainty) {
    const char* topics[] = {"topic_a", "topic_b", "topic_c", "topic_d"};
    char selected[256];

    float uncertainty = curiosity_enhanced_sample_by_uncertainty(
        system, topics, 4, selected);

    EXPECT_GE(uncertainty, 0.0f);
    EXPECT_GT(strlen(selected), 0u);

    // Selected should be one of the input topics
    bool found = false;
    for (int i = 0; i < 4; i++) {
        if (strcmp(selected, topics[i]) == 0) {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
}

TEST_F(CuriosityQMCTest, SampleByUncertaintyNullParams) {
    const char* topics[] = {"topic"};
    char selected[256];

    EXPECT_LT(curiosity_enhanced_sample_by_uncertainty(nullptr, topics, 1, selected), 0.0f);
    EXPECT_LT(curiosity_enhanced_sample_by_uncertainty(system, nullptr, 1, selected), 0.0f);
    EXPECT_LT(curiosity_enhanced_sample_by_uncertainty(system, topics, 0, selected), 0.0f);
    EXPECT_LT(curiosity_enhanced_sample_by_uncertainty(system, topics, 1, nullptr), 0.0f);
}

TEST_F(CuriosityQMCTest, GetExplorationBonus) {
    float bonus = curiosity_enhanced_get_exploration_bonus(system, "unvisited_topic");

    EXPECT_GE(bonus, 0.0f);
    EXPECT_LE(bonus, 1.0f);
}

TEST_F(CuriosityQMCTest, GetExplorationBonusNullParams) {
    EXPECT_EQ(curiosity_enhanced_get_exploration_bonus(nullptr, "topic"), 0.0f);
    EXPECT_EQ(curiosity_enhanced_get_exploration_bonus(system, nullptr), 0.0f);
}

TEST_F(CuriosityQMCTest, UpdateInterestMC) {
    float updated = curiosity_enhanced_update_interest_mc(
        system, "test_topic", 0.8f, 0.1f);

    EXPECT_GE(updated, 0.0f);
    EXPECT_LE(updated, 1.0f);
}

TEST_F(CuriosityQMCTest, UpdateInterestMCClamping) {
    // Test clamping of observed_interest
    float updated1 = curiosity_enhanced_update_interest_mc(system, "topic1", 1.5f, 0.1f);
    EXPECT_LE(updated1, 1.0f);

    float updated2 = curiosity_enhanced_update_interest_mc(system, "topic2", -0.5f, 0.1f);
    EXPECT_GE(updated2, 0.0f);
}

TEST_F(CuriosityQMCTest, UpdateInterestMCNullParams) {
    EXPECT_EQ(curiosity_enhanced_update_interest_mc(nullptr, "topic", 0.5f, 0.1f), 0.0f);
    EXPECT_EQ(curiosity_enhanced_update_interest_mc(system, nullptr, 0.5f, 0.1f), 0.0f);
}

TEST_F(CuriosityQMCTest, EstimateInfoGainQMC) {
    float info_gain = curiosity_enhanced_estimate_info_gain_qmc(system, "test_topic");

    EXPECT_GE(info_gain, 0.0f);
    EXPECT_LE(info_gain, 5.0f);  // Capped at 5 bits
}

TEST_F(CuriosityQMCTest, EstimateInfoGainQMCNullParams) {
    EXPECT_EQ(curiosity_enhanced_estimate_info_gain_qmc(nullptr, "topic"), 0.0f);
    EXPECT_EQ(curiosity_enhanced_estimate_info_gain_qmc(system, nullptr), 0.0f);
}

TEST_F(CuriosityQMCTest, GetQMCStats) {
    // Run some operations to generate stats
    curiosity_qmc_uncertainty_t uncertainty;
    curiosity_enhanced_estimate_uncertainty(system, "topic1", &uncertainty);
    curiosity_enhanced_estimate_uncertainty(system, "topic2", &uncertainty);

    curiosity_empowerment_result_t emp;
    curiosity_enhanced_compute_empowerment(system, "topic1", 3, &emp);
    curiosity_empowerment_result_free(&emp);

    curiosity_qmc_stats_t stats;
    int ret = curiosity_enhanced_get_qmc_stats(system, &stats);

    EXPECT_EQ(ret, 0);
    EXPECT_GE(stats.uncertainty_estimations, 2u);
    EXPECT_GE(stats.empowerment_calculations, 1u);
    EXPECT_GT(stats.mc_samples_total, 0u);
}

TEST_F(CuriosityQMCTest, GetQMCStatsNullParams) {
    curiosity_qmc_stats_t stats;

    EXPECT_NE(curiosity_enhanced_get_qmc_stats(nullptr, &stats), 0);
    EXPECT_NE(curiosity_enhanced_get_qmc_stats(system, nullptr), 0);
}

TEST_F(CuriosityQMCTest, ResetQMCStats) {
    // Generate some stats
    curiosity_qmc_uncertainty_t uncertainty;
    curiosity_enhanced_estimate_uncertainty(system, "topic", &uncertainty);

    // Reset
    curiosity_enhanced_reset_qmc_stats(system);

    // Verify reset
    curiosity_qmc_stats_t stats;
    curiosity_enhanced_get_qmc_stats(system, &stats);

    EXPECT_EQ(stats.uncertainty_estimations, 0u);
    EXPECT_EQ(stats.empowerment_calculations, 0u);
    EXPECT_EQ(stats.mc_samples_total, 0u);
}

TEST_F(CuriosityQMCTest, ResetQMCStatsNullSafe) {
    curiosity_enhanced_reset_qmc_stats(nullptr);  // Should not crash
}

/* ============================================================================
 * Base Curiosity Empowerment Tests
 * ============================================================================ */

TEST_F(CuriosityBaseQMCTest, ComputeEmpowermentBasic) {
    curiosity_empowerment_t result;
    int ret = curiosity_compute_empowerment(engine, "test_concept", 3, &result);

    EXPECT_EQ(ret, 0);
    EXPECT_GE(result.empowerment, 0.0f);
    EXPECT_GE(result.empowerment_normalized, 0.0f);
    EXPECT_LE(result.empowerment_normalized, 1.0f);
    EXPECT_GT(result.action_count, 0u);
}

TEST_F(CuriosityBaseQMCTest, ComputeEmpowermentDefaultHorizon) {
    curiosity_empowerment_t result;
    int ret = curiosity_compute_empowerment(engine, "concept", 0, &result);

    EXPECT_EQ(ret, 0);
    EXPECT_GE(result.empowerment, 0.0f);
}

TEST_F(CuriosityBaseQMCTest, ComputeEmpowermentNullParams) {
    curiosity_empowerment_t result;

    EXPECT_EQ(curiosity_compute_empowerment(nullptr, "concept", 3, &result), -1);
    EXPECT_EQ(curiosity_compute_empowerment(engine, nullptr, 3, &result), -1);
    EXPECT_EQ(curiosity_compute_empowerment(engine, "concept", 3, nullptr), -1);
}

TEST_F(CuriosityBaseQMCTest, SampleByEmpowerment) {
    const char* concepts[] = {"concept_a", "concept_b", "concept_c"};

    uint32_t selected = curiosity_sample_by_empowerment(engine, concepts, 3, 1.0f);

    EXPECT_LT(selected, 3u);
}

TEST_F(CuriosityBaseQMCTest, SampleByEmpowermentHighTemp) {
    const char* concepts[] = {"c1", "c2", "c3", "c4"};

    // High temperature should give more uniform sampling
    uint32_t selected = curiosity_sample_by_empowerment(engine, concepts, 4, 10.0f);
    EXPECT_LT(selected, 4u);
}

TEST_F(CuriosityBaseQMCTest, SampleByEmpowermentLowTemp) {
    const char* concepts[] = {"c1", "c2"};

    // Low temperature should be more deterministic
    uint32_t selected = curiosity_sample_by_empowerment(engine, concepts, 2, 0.1f);
    EXPECT_LT(selected, 2u);
}

TEST_F(CuriosityBaseQMCTest, SampleByEmpowermentNullParams) {
    const char* concepts[] = {"concept"};

    EXPECT_EQ(curiosity_sample_by_empowerment(nullptr, concepts, 1, 1.0f), 0u);
    EXPECT_EQ(curiosity_sample_by_empowerment(engine, nullptr, 1, 1.0f), 0u);
    EXPECT_EQ(curiosity_sample_by_empowerment(engine, concepts, 0, 1.0f), 0u);
}

TEST_F(CuriosityBaseQMCTest, ComputeIntrinsicReward) {
    float reward = curiosity_compute_intrinsic_reward(engine, "test_concept", 0.5f, 0.5f);

    EXPECT_GE(reward, 0.0f);
    EXPECT_LE(reward, 1.0f);
}

TEST_F(CuriosityBaseQMCTest, ComputeIntrinsicRewardPureEmpowerment) {
    float reward = curiosity_compute_intrinsic_reward(engine, "concept", 1.0f, 0.0f);

    EXPECT_GE(reward, 0.0f);
    EXPECT_LE(reward, 1.0f);
}

TEST_F(CuriosityBaseQMCTest, ComputeIntrinsicRewardPureNovelty) {
    float reward = curiosity_compute_intrinsic_reward(engine, "concept", 0.0f, 1.0f);

    EXPECT_GE(reward, 0.0f);
    EXPECT_LE(reward, 1.0f);
}

TEST_F(CuriosityBaseQMCTest, ComputeIntrinsicRewardNormalization) {
    // Weights > 1 should be normalized
    float reward = curiosity_compute_intrinsic_reward(engine, "concept", 2.0f, 2.0f);

    EXPECT_GE(reward, 0.0f);
    EXPECT_LE(reward, 1.0f);
}

TEST_F(CuriosityBaseQMCTest, ComputeIntrinsicRewardNullParams) {
    EXPECT_EQ(curiosity_compute_intrinsic_reward(nullptr, "concept", 0.5f, 0.5f), 0.0f);
    EXPECT_EQ(curiosity_compute_intrinsic_reward(engine, nullptr, 0.5f, 0.5f), 0.0f);
}

TEST_F(CuriosityBaseQMCTest, EstimateEmpowermentChange) {
    float change = curiosity_estimate_empowerment_change(engine, "concept", 100);

    // Empowerment change can be positive or negative but typically around 0.1
    EXPECT_GT(change, -1.0f);
    EXPECT_LT(change, 1.0f);
}

TEST_F(CuriosityBaseQMCTest, EstimateEmpowermentChangeDefaultSims) {
    float change = curiosity_estimate_empowerment_change(engine, "concept", 0);

    EXPECT_GT(change, -1.0f);
    EXPECT_LT(change, 1.0f);
}

TEST_F(CuriosityBaseQMCTest, EstimateEmpowermentChangeNullParams) {
    EXPECT_EQ(curiosity_estimate_empowerment_change(nullptr, "concept", 100), 0.0f);
    EXPECT_EQ(curiosity_estimate_empowerment_change(engine, nullptr, 100), 0.0f);
}

/* ============================================================================
 * Determinism and Reproducibility Tests
 * ============================================================================ */

TEST_F(CuriosityQMCTest, UncertaintyDeterministicWithSeed) {
    curiosity_qmc_config_t qmc_config;
    curiosity_enhanced_qmc_default_config(&qmc_config);
    qmc_config.seed = 42;
    curiosity_enhanced_set_qmc_config(system, &qmc_config);

    curiosity_qmc_uncertainty_t result1, result2;
    curiosity_enhanced_estimate_uncertainty(system, "topic", &result1);

    // Reset seed
    qmc_config.seed = 42;
    curiosity_enhanced_set_qmc_config(system, &qmc_config);

    curiosity_enhanced_estimate_uncertainty(system, "topic", &result2);

    // With same seed, results should be identical
    EXPECT_FLOAT_EQ(result1.mean_interest, result2.mean_interest);
    EXPECT_FLOAT_EQ(result1.variance, result2.variance);
}

/* ============================================================================
 * Bounds and Edge Cases Tests
 * ============================================================================ */

TEST_F(CuriosityQMCTest, ConfidenceIntervalBounds) {
    curiosity_qmc_uncertainty_t result;
    curiosity_enhanced_estimate_uncertainty(system, "topic", &result);

    EXPECT_GE(result.confidence_95_lower, 0.0f);
    EXPECT_LE(result.confidence_95_lower, 1.0f);
    EXPECT_GE(result.confidence_95_upper, 0.0f);
    EXPECT_LE(result.confidence_95_upper, 1.0f);
    EXPECT_LE(result.confidence_95_lower, result.confidence_95_upper);
}

TEST_F(CuriosityQMCTest, EmpowermentBounds) {
    curiosity_empowerment_result_t result;
    curiosity_enhanced_compute_empowerment(system, "topic", 5, &result);

    EXPECT_GE(result.empowerment, 0.0f);
    EXPECT_GE(result.empowerment_normalized, 0.0f);
    EXPECT_LE(result.empowerment_normalized, 1.0f);

    // Empowerment should be <= channel capacity
    EXPECT_LE(result.empowerment, result.channel_capacity + 0.001f);

    curiosity_empowerment_result_free(&result);
}

/* ============================================================================
 * Memory Safety Tests
 * ============================================================================ */

TEST_F(CuriosityQMCTest, EmpowermentResultCleanup) {
    curiosity_empowerment_result_t result;
    memset(&result, 0, sizeof(result));

    int ret = curiosity_enhanced_compute_empowerment(system, "topic", 3, &result);
    EXPECT_EQ(ret, 0);
    EXPECT_NE(result.action_empowerment, nullptr);

    // Free should work without issues
    curiosity_empowerment_result_free(&result);
    EXPECT_EQ(result.action_empowerment, nullptr);
}

TEST_F(CuriosityQMCTest, MultipleEmpowermentAllocations) {
    for (int i = 0; i < 10; i++) {
        curiosity_empowerment_result_t result;
        int ret = curiosity_enhanced_compute_empowerment(system, "topic", 3, &result);
        EXPECT_EQ(ret, 0);
        curiosity_empowerment_result_free(&result);
    }
}
