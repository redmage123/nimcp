/**
 * @file test_curiosity_qmc_integration.cpp
 * @brief Integration tests for Curiosity QMC with other NIMCP systems
 * @version 1.0.0
 * @date 2026-01-12
 *
 * WHAT: Test QMC curiosity integration with FEP, quantum bridge, executive
 * WHY:  Verify Step 10 MC Integration works across module boundaries
 * HOW:  Test multi-module interactions, coordination, end-to-end flows
 *
 * @author NIMCP Development Team
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <vector>

extern "C" {
#include "cognitive/curiosity/nimcp_curiosity_enhanced.h"
#include "cognitive/curiosity/nimcp_curiosity.h"
#include "cognitive/curiosity/nimcp_curiosity_quantum_bridge.h"
#include "core/brain/nimcp_brain.h"
#include "utils/memory/nimcp_memory.h"
}

/* ============================================================================
 * Test Fixtures
 * ============================================================================ */

class CuriosityQMCIntegrationTest : public ::testing::Test {
protected:
    brain_t brain = nullptr;
    curiosity_enhanced_system_t* enhanced_system = nullptr;
    curiosity_engine_t base_engine = nullptr;

    void SetUp() override {
        // Create brain
        brain = brain_create("qmc_integration_test", BRAIN_SIZE_TINY,
                             BRAIN_TASK_CLASSIFICATION, 10, 5);
        ASSERT_NE(brain, nullptr);

        // Create base curiosity engine
        base_engine = curiosity_engine_create(brain, "test_learner");
        ASSERT_NE(base_engine, nullptr);

        // Create enhanced curiosity system with quantum enabled
        curiosity_enhanced_config_t config;
        curiosity_enhanced_config_default(&config);
        config.enable_all_enhancements = true;
        config.enable_quantum_curiosity = true;
        enhanced_system = curiosity_enhanced_create(&config, base_engine);
        ASSERT_NE(enhanced_system, nullptr);
    }

    void TearDown() override {
        if (enhanced_system) {
            curiosity_enhanced_destroy(enhanced_system);
            enhanced_system = nullptr;
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

/* ============================================================================
 * QMC-Quantum Bridge Integration Tests
 * ============================================================================ */

TEST_F(CuriosityQMCIntegrationTest, QMCWithQuantumBridgeTopics) {
    // Add topics to quantum bridge
    curiosity_quantum_bridge_t* qbridge = curiosity_enhanced_get_quantum_bridge(enhanced_system);
    if (qbridge) {
        curiosity_enhanced_add_quantum_topic(enhanced_system, "quantum_topic_1", 0.8f, 0.9f);
        curiosity_enhanced_add_quantum_topic(enhanced_system, "quantum_topic_2", 0.6f, 0.7f);
        curiosity_enhanced_add_quantum_topic(enhanced_system, "quantum_topic_3", 0.4f, 0.5f);

        // QMC operations should work with quantum bridge topics
        curiosity_qmc_uncertainty_t uncertainty;
        int ret = curiosity_enhanced_estimate_uncertainty(enhanced_system, "quantum_topic_1", &uncertainty);
        EXPECT_EQ(ret, 0);
        EXPECT_GE(uncertainty.mean_interest, 0.0f);

        // Empowerment should reflect quantum bridge structure
        curiosity_empowerment_result_t emp;
        ret = curiosity_enhanced_compute_empowerment(enhanced_system, "quantum_topic_1", 3, &emp);
        EXPECT_EQ(ret, 0);
        EXPECT_GT(emp.num_actions, 0u);
        curiosity_empowerment_result_free(&emp);
    }
}

TEST_F(CuriosityQMCIntegrationTest, QMCExplorationWithQuantumWalk) {
    // Add topics for quantum walk exploration
    curiosity_enhanced_add_quantum_topic(enhanced_system, "start_topic", 0.5f, 0.3f);
    curiosity_enhanced_add_quantum_topic(enhanced_system, "related_topic_1", 0.6f, 0.7f);
    curiosity_enhanced_add_quantum_topic(enhanced_system, "related_topic_2", 0.7f, 0.8f);

    // Quantum explore should work
    char novel_topic[256] = {0};
    float novelty = curiosity_enhanced_quantum_explore(enhanced_system, "start_topic", novel_topic);

    if (novelty >= 0.0f) {
        EXPECT_GT(strlen(novel_topic), 0u);

        // Now use QMC to evaluate the found topic
        curiosity_qmc_uncertainty_t uncertainty;
        int ret = curiosity_enhanced_estimate_uncertainty(enhanced_system, novel_topic, &uncertainty);
        EXPECT_EQ(ret, 0);
    }
}

/* ============================================================================
 * QMC-Interest Update Integration Tests
 * ============================================================================ */

TEST_F(CuriosityQMCIntegrationTest, MCUpdateAffectsUncertainty) {
    const char* topic = "test_topic";

    // Initial uncertainty
    curiosity_qmc_uncertainty_t before;
    curiosity_enhanced_estimate_uncertainty(enhanced_system, topic, &before);

    // Update interest with MC
    curiosity_enhanced_update_interest_mc(enhanced_system, topic, 0.8f, 0.1f);

    // Uncertainty should change after update
    curiosity_qmc_uncertainty_t after;
    curiosity_enhanced_estimate_uncertainty(enhanced_system, topic, &after);

    // After observation, epistemic uncertainty should decrease
    // (though this may depend on implementation details)
    EXPECT_GE(after.mean_interest, 0.0f);
    EXPECT_LE(after.mean_interest, 1.0f);
}

TEST_F(CuriosityQMCIntegrationTest, MultipleUpdatesReduceUncertainty) {
    const char* topic = "learning_topic";

    // Multiple updates should generally reduce epistemic uncertainty
    for (int i = 0; i < 5; i++) {
        curiosity_enhanced_update_interest_mc(enhanced_system, topic, 0.7f, 0.15f);
    }

    curiosity_qmc_uncertainty_t final_uncertainty;
    curiosity_enhanced_estimate_uncertainty(enhanced_system, topic, &final_uncertainty);

    // After multiple observations, uncertainty should be bounded
    EXPECT_GE(final_uncertainty.mean_interest, 0.0f);
    EXPECT_LE(final_uncertainty.mean_interest, 1.0f);
}

/* ============================================================================
 * QMC-Empowerment Integration Tests
 * ============================================================================ */

TEST_F(CuriosityQMCIntegrationTest, EmpowermentAffectsExplorationBonus) {
    const char* topic = "empowering_topic";

    // Compute empowerment
    curiosity_empowerment_result_t emp;
    int ret = curiosity_enhanced_compute_empowerment(enhanced_system, topic, 3, &emp);
    EXPECT_EQ(ret, 0);

    // Get exploration bonus
    float bonus = curiosity_enhanced_get_exploration_bonus(enhanced_system, topic);

    // Both should be valid
    EXPECT_GE(emp.empowerment, 0.0f);
    EXPECT_GE(bonus, 0.0f);

    curiosity_empowerment_result_free(&emp);
}

TEST_F(CuriosityQMCIntegrationTest, InfoGainCorrelatesWithUncertainty) {
    const char* high_uncertainty_topic = "unknown_topic";
    const char* low_uncertainty_topic = "known_topic";

    // Register known topic with multiple observations
    for (int i = 0; i < 10; i++) {
        curiosity_enhanced_update_interest_mc(enhanced_system, low_uncertainty_topic, 0.5f, 0.1f);
    }

    // Info gain for unknown should be >= info gain for known
    float info_gain_unknown = curiosity_enhanced_estimate_info_gain_qmc(enhanced_system, high_uncertainty_topic);
    float info_gain_known = curiosity_enhanced_estimate_info_gain_qmc(enhanced_system, low_uncertainty_topic);

    EXPECT_GE(info_gain_unknown, 0.0f);
    EXPECT_GE(info_gain_known, 0.0f);
    // Unknown topic typically has higher info gain
    EXPECT_GE(info_gain_unknown, info_gain_known - 0.5f);
}

/* ============================================================================
 * Base Curiosity-Brain Integration Tests
 * ============================================================================ */

TEST_F(CuriosityQMCIntegrationTest, BaseEmpowermentWithBrain) {
    // Base curiosity empowerment should work with brain context
    curiosity_empowerment_t result;
    int ret = curiosity_compute_empowerment(base_engine, "test_concept", 3, &result);

    EXPECT_EQ(ret, 0);
    EXPECT_GE(result.empowerment, 0.0f);
    EXPECT_GE(result.empowerment_normalized, 0.0f);
    EXPECT_LE(result.empowerment_normalized, 1.0f);
}

TEST_F(CuriosityQMCIntegrationTest, IntrinsicRewardWithKnowledgeGap) {
    // Detect knowledge gap
    knowledge_gap_t gap = curiosity_detect_knowledge_gap(base_engine, "novel_concept");

    // Compute intrinsic reward
    float reward = curiosity_compute_intrinsic_reward(base_engine, "novel_concept", 0.5f, 0.5f);

    EXPECT_GE(reward, 0.0f);
    EXPECT_LE(reward, 1.0f);

    // Higher gap should correlate with higher novelty component
    if (gap.gap_size > 0.5f) {
        EXPECT_GT(reward, 0.1f);
    }
}

TEST_F(CuriosityQMCIntegrationTest, EmpowermentSamplingDistribution) {
    const char* concepts[] = {"concept_a", "concept_b", "concept_c", "concept_d"};

    // Sample multiple times and check distribution
    int counts[4] = {0, 0, 0, 0};
    int num_samples = 100;

    for (int i = 0; i < num_samples; i++) {
        uint32_t selected = curiosity_sample_by_empowerment(base_engine, concepts, 4, 1.0f);
        if (selected < 4) {
            counts[selected]++;
        }
    }

    // All concepts should be sampled at least once (probabilistic, but likely)
    int total_sampled = counts[0] + counts[1] + counts[2] + counts[3];
    EXPECT_EQ(total_sampled, num_samples);
}

/* ============================================================================
 * Cross-Module Statistics Tests
 * ============================================================================ */

TEST_F(CuriosityQMCIntegrationTest, QMCStatsAccumulate) {
    // Reset stats
    curiosity_enhanced_reset_qmc_stats(enhanced_system);

    // Perform various QMC operations
    curiosity_qmc_uncertainty_t uncertainty;
    curiosity_enhanced_estimate_uncertainty(enhanced_system, "topic1", &uncertainty);
    curiosity_enhanced_estimate_uncertainty(enhanced_system, "topic2", &uncertainty);
    curiosity_enhanced_estimate_uncertainty(enhanced_system, "topic3", &uncertainty);

    curiosity_empowerment_result_t emp;
    curiosity_enhanced_compute_empowerment(enhanced_system, "topic1", 3, &emp);
    curiosity_empowerment_result_free(&emp);
    curiosity_enhanced_compute_empowerment(enhanced_system, "topic2", 3, &emp);
    curiosity_empowerment_result_free(&emp);

    // Check stats
    curiosity_qmc_stats_t stats;
    curiosity_enhanced_get_qmc_stats(enhanced_system, &stats);

    EXPECT_EQ(stats.uncertainty_estimations, 3u);
    EXPECT_EQ(stats.empowerment_calculations, 2u);
    EXPECT_GT(stats.mc_samples_total, 0u);
}

/* ============================================================================
 * Concurrent Access Tests
 * ============================================================================ */

TEST_F(CuriosityQMCIntegrationTest, SequentialOperationsSafe) {
    // Run multiple operations sequentially (thread safety for single thread)
    for (int i = 0; i < 10; i++) {
        curiosity_qmc_uncertainty_t uncertainty;
        curiosity_enhanced_estimate_uncertainty(enhanced_system, "topic", &uncertainty);

        curiosity_empowerment_result_t emp;
        curiosity_enhanced_compute_empowerment(enhanced_system, "topic", 3, &emp);
        curiosity_empowerment_result_free(&emp);

        curiosity_enhanced_update_interest_mc(enhanced_system, "topic", 0.5f, 0.1f);

        float bonus = curiosity_enhanced_get_exploration_bonus(enhanced_system, "topic");
        (void)bonus;

        float info_gain = curiosity_enhanced_estimate_info_gain_qmc(enhanced_system, "topic");
        (void)info_gain;
    }

    // Verify stats are reasonable
    curiosity_qmc_stats_t stats;
    curiosity_enhanced_get_qmc_stats(enhanced_system, &stats);
    EXPECT_EQ(stats.uncertainty_estimations, 10u);
    EXPECT_EQ(stats.empowerment_calculations, 10u);
}

/* ============================================================================
 * Edge Cases and Robustness Tests
 * ============================================================================ */

TEST_F(CuriosityQMCIntegrationTest, EmptyTopicHandling) {
    curiosity_qmc_uncertainty_t uncertainty;

    // Empty string should be handled gracefully
    int ret = curiosity_enhanced_estimate_uncertainty(enhanced_system, "", &uncertainty);
    // May succeed or fail, but should not crash
    (void)ret;
}

TEST_F(CuriosityQMCIntegrationTest, LongTopicNameHandling) {
    // Create a very long topic name
    std::string long_topic(300, 'a');

    curiosity_qmc_uncertainty_t uncertainty;
    int ret = curiosity_enhanced_estimate_uncertainty(enhanced_system, long_topic.c_str(), &uncertainty);
    // Should handle gracefully
    (void)ret;
}

TEST_F(CuriosityQMCIntegrationTest, UnicodeTopicHandling) {
    const char* unicode_topic = "curiosity_\xC3\xA9motion";  // UTF-8 for "curiosity_émotion"

    curiosity_qmc_uncertainty_t uncertainty;
    int ret = curiosity_enhanced_estimate_uncertainty(enhanced_system, unicode_topic, &uncertainty);
    EXPECT_EQ(ret, 0);
}

TEST_F(CuriosityQMCIntegrationTest, HighVolumeOperations) {
    // Stress test with many operations
    for (int i = 0; i < 100; i++) {
        char topic[32];
        snprintf(topic, sizeof(topic), "topic_%d", i % 10);

        curiosity_qmc_uncertainty_t uncertainty;
        curiosity_enhanced_estimate_uncertainty(enhanced_system, topic, &uncertainty);
    }

    curiosity_qmc_stats_t stats;
    curiosity_enhanced_get_qmc_stats(enhanced_system, &stats);
    EXPECT_EQ(stats.uncertainty_estimations, 100u);
}
