/**
 * @file test_reasoning_calibration_e2e.cpp
 * @brief End-to-end tests for confidence calibration learning
 *
 * WHAT: Tests calibration learning over multiple queries with a full brain
 * WHY:  Verify that calibration actually learns and improves over time
 * HOW:  Creates brain, connects engine with calibration, runs queries,
 *       records outcomes, verifies calibration adapts
 */

#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "cognitive/reasoning/nimcp_reasoning_calibration.h"
#include "cognitive/reasoning/nimcp_reasoning_convergent.h"
#include "cognitive/reasoning/nimcp_reasoning_chain.h"
#include "core/brain/nimcp_brain.h"
}

class CalibrationE2ETest : public ::testing::Test {
protected:
    brain_t brain = NULL;

    void SetUp() override {
        brain = brain_create("calibration_e2e", BRAIN_SIZE_SMALL,
                             BRAIN_TASK_CLASSIFICATION, 4, 2);
    }

    void TearDown() override {
        if (brain) {
            brain_destroy(brain);
            brain = NULL;
        }
    }
};

TEST_F(CalibrationE2ETest, CalibrationLearningOverTime) {
    /* Test that calibration system learns contributor reliability */
    calibration_config_t config = reasoning_calibration_default_config();
    config.min_predictions_before_adjust = 3;
    config.learning_rate = 0.15f;
    reasoning_calibration_t* cal = reasoning_calibration_create(&config);
    ASSERT_NE(cal, nullptr);

    /* Simulate 20 queries with known outcomes */
    const char* contributors[] = {
        "hippocampus", "semantic_memory", "parietal",
        "intuition", "creative"
    };
    /* Hippocampus and semantic are reliable; intuition is unreliable */
    float reliability[] = { 0.9f, 0.85f, 0.7f, 0.3f, 0.5f };

    for (int q = 0; q < 20; q++) {
        for (int c = 0; c < 5; c++) {
            float pred_conf = 0.6f + (float)(q % 3) * 0.1f;
            /* Simulate outcome based on per-contributor reliability */
            float actual = ((float)(q * 7 + c * 3) / 100.0f < reliability[c])
                           ? 1.0f : 0.0f;
            reasoning_calibration_record(cal, contributors[c],
                                          pred_conf, actual);
        }
    }

    /* Verify calibration learned correct reliability ordering */
    contributor_calibration_t hip_stats, int_stats;
    reasoning_calibration_get_contributor_stats(cal, "hippocampus", &hip_stats);
    reasoning_calibration_get_contributor_stats(cal, "intuition", &int_stats);

    /* Hippocampus should be more reliable than intuition */
    EXPECT_GT(hip_stats.reliability_score, int_stats.reliability_score);

    /* Hippocampus should have higher scale than intuition */
    EXPECT_GT(hip_stats.confidence_scale, int_stats.confidence_scale);

    /* Global stats should reflect the learning */
    calibration_stats_t stats;
    reasoning_calibration_get_stats(cal, &stats);
    EXPECT_EQ(stats.total_contributors_tracked, 5u);
    EXPECT_EQ(stats.total_records, 100u); /* 20 * 5 */
    EXPECT_GT(stats.avg_reliability, 0.0f);

    reasoning_calibration_destroy(cal);
}

TEST_F(CalibrationE2ETest, CalibrationWithFullBrain) {
    if (!brain) GTEST_SKIP() << "Brain creation failed";

    /* Create engine with calibration enabled */
    reasoning_engine_config_t eng_config = reasoning_engine_default_config();
    eng_config.enable_calibration = true;
    eng_config.calibration_learning_rate = 0.1f;

    reasoning_engine_t* engine = reasoning_engine_create(&eng_config);
    ASSERT_NE(engine, nullptr);

    int rc = reasoning_engine_connect_brain(engine, brain);
    ASSERT_EQ(rc, 0);

    /* Run multiple queries */
    const char* queries[] = {
        "What is consciousness?",
        "How does learning work?",
        "Why do neurons fire?",
        "Is memory distributed?",
        "What causes neuroplasticity?",
    };

    for (int i = 0; i < 5; i++) {
        reasoning_chain_t chain;
        reasoning_chain_init(&chain);
        chain.start_time_us = 0;

        rc = reasoning_engine_reason(engine, queries[i], &chain);
        EXPECT_EQ(rc, 0) << "Query " << i << " failed: " << queries[i];
        EXPECT_TRUE(chain.is_complete)
            << "Query " << i << " incomplete: " << queries[i];
        EXPECT_GE(chain.overall_confidence, 0.0f);

        reasoning_chain_cleanup(&chain);
    }

    /* Verify stats */
    reasoning_engine_stats_t stats;
    reasoning_engine_get_stats(engine, &stats);
    EXPECT_GE(stats.total_queries, 5u);

    reasoning_engine_destroy(engine);
}

TEST_F(CalibrationE2ETest, CalibrationResetAndRelearn) {
    /* Calibrate, reset, recalibrate — verify fresh start */
    calibration_config_t config = reasoning_calibration_default_config();
    config.min_predictions_before_adjust = 2;
    config.learning_rate = 0.3f;
    reasoning_calibration_t* cal = reasoning_calibration_create(&config);
    ASSERT_NE(cal, nullptr);

    /* Phase 1: Train with hippocampus as reliable */
    for (int i = 0; i < 10; i++) {
        reasoning_calibration_record(cal, "hippocampus", 0.8f, 1.0f);
    }

    contributor_calibration_t hip_stats;
    reasoning_calibration_get_contributor_stats(cal, "hippocampus", &hip_stats);
    float original_scale = hip_stats.confidence_scale;
    EXPECT_GT(original_scale, 0.5f);

    /* Reset */
    int rc = reasoning_calibration_reset(cal);
    EXPECT_EQ(rc, 0);

    /* Verify contributor is gone */
    rc = reasoning_calibration_get_contributor_stats(
        cal, "hippocampus", &hip_stats);
    EXPECT_EQ(rc, -1);

    /* Phase 2: Retrain with hippocampus as unreliable */
    for (int i = 0; i < 10; i++) {
        reasoning_calibration_record(cal, "hippocampus", 0.9f, 0.0f);
    }

    reasoning_calibration_get_contributor_stats(cal, "hippocampus", &hip_stats);
    /* After reset and retrain as unreliable, scale should be low */
    EXPECT_LT(hip_stats.confidence_scale, original_scale);
    EXPECT_FLOAT_EQ(hip_stats.reliability_score, 0.0f);

    reasoning_calibration_destroy(cal);
}
