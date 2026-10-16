/**
 * @file test_reasoning_calibration.cpp
 * @brief Unit tests for confidence calibration learning system
 *
 * WHAT: Tests calibration create/destroy, recording, adjustments, stats, reset
 * WHY:  Verify calibration system works correctly in isolation
 * HOW:  GTest suite testing each function independently
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <thread>
#include <vector>

extern "C" {
#include "cognitive/reasoning/nimcp_reasoning_calibration.h"
#include "cognitive/reasoning/nimcp_reasoning_chain.h"
}

/*=============================================================================
 * TEST FIXTURE
 *===========================================================================*/

class ReasoningCalibrationTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

/*=============================================================================
 * LIFECYCLE TESTS
 *===========================================================================*/

TEST_F(ReasoningCalibrationTest, CreateDestroy) {
    calibration_config_t config = reasoning_calibration_default_config();
    reasoning_calibration_t* cal = reasoning_calibration_create(&config);
    ASSERT_NE(cal, nullptr);
    reasoning_calibration_destroy(cal);
}

TEST_F(ReasoningCalibrationTest, CreateNull) {
    /* NULL config should use defaults */
    reasoning_calibration_t* cal = reasoning_calibration_create(NULL);
    ASSERT_NE(cal, nullptr);
    reasoning_calibration_destroy(cal);
}

TEST_F(ReasoningCalibrationTest, DestroyNull) {
    /* Should not crash */
    reasoning_calibration_destroy(NULL);
}

/*=============================================================================
 * DEFAULT CONFIG TESTS
 *===========================================================================*/

TEST_F(ReasoningCalibrationTest, DefaultConfig) {
    calibration_config_t config = reasoning_calibration_default_config();

    EXPECT_TRUE(config.enabled);
    EXPECT_FLOAT_EQ(config.learning_rate, REASONING_DEFAULT_CALIBRATION_LEARNING_RATE);
    EXPECT_EQ(config.history_size, (uint32_t)REASONING_DEFAULT_CALIBRATION_HISTORY);
    EXPECT_EQ(config.min_predictions_before_adjust, 5u);
    EXPECT_FLOAT_EQ(config.max_scale, 2.0f);
    EXPECT_FLOAT_EQ(config.min_scale, 0.1f);
}

/*=============================================================================
 * RECORDING TESTS
 *===========================================================================*/

TEST_F(ReasoningCalibrationTest, RecordSinglePrediction) {
    reasoning_calibration_t* cal = reasoning_calibration_create(NULL);
    ASSERT_NE(cal, nullptr);

    int rc = reasoning_calibration_record(cal, "hippocampus", 0.7f, 1.0f);
    EXPECT_EQ(rc, 0);

    /* Verify contributor was created */
    contributor_calibration_t stats;
    rc = reasoning_calibration_get_contributor_stats(cal, "hippocampus", &stats);
    EXPECT_EQ(rc, 0);
    EXPECT_EQ(stats.total_predictions, 1u);
    EXPECT_EQ(stats.correct_predictions, 1u);
    EXPECT_FLOAT_EQ(stats.reliability_score, 1.0f);

    reasoning_calibration_destroy(cal);
}

TEST_F(ReasoningCalibrationTest, RecordMultiplePredictions) {
    reasoning_calibration_t* cal = reasoning_calibration_create(NULL);
    ASSERT_NE(cal, nullptr);

    /* Record 10 predictions for the same contributor */
    for (int i = 0; i < 10; i++) {
        float outcome = (i % 3 == 0) ? 0.0f : 1.0f; /* 3 wrong, 7 correct */
        reasoning_calibration_record(cal, "semantic_memory", 0.6f, outcome);
    }

    contributor_calibration_t stats;
    int rc = reasoning_calibration_get_contributor_stats(
        cal, "semantic_memory", &stats);
    EXPECT_EQ(rc, 0);
    EXPECT_EQ(stats.total_predictions, 10u);
    /* Predictions at i=0,3,6,9 are wrong (outcome=0) -> 4 wrong, 6 correct */
    EXPECT_EQ(stats.correct_predictions, 6u);
    EXPECT_NEAR(stats.reliability_score, 0.6f, 0.01f);

    reasoning_calibration_destroy(cal);
}

TEST_F(ReasoningCalibrationTest, RecordMultipleContributors) {
    reasoning_calibration_t* cal = reasoning_calibration_create(NULL);
    ASSERT_NE(cal, nullptr);

    reasoning_calibration_record(cal, "hippocampus", 0.5f, 1.0f);
    reasoning_calibration_record(cal, "parietal", 0.8f, 0.0f);
    reasoning_calibration_record(cal, "intuition", 0.3f, 1.0f);

    calibration_stats_t global_stats;
    int rc = reasoning_calibration_get_stats(cal, &global_stats);
    EXPECT_EQ(rc, 0);
    EXPECT_EQ(global_stats.total_contributors_tracked, 3u);
    EXPECT_EQ(global_stats.total_records, 3u);

    reasoning_calibration_destroy(cal);
}

/*=============================================================================
 * ADJUSTMENT TESTS
 *===========================================================================*/

TEST_F(ReasoningCalibrationTest, GetAdjustmentUnknownContributor) {
    reasoning_calibration_t* cal = reasoning_calibration_create(NULL);
    ASSERT_NE(cal, nullptr);

    float scale = 0.0f, bias = 999.0f;
    int rc = reasoning_calibration_get_adjustment(
        cal, "nonexistent", &scale, &bias);
    EXPECT_EQ(rc, 0);
    EXPECT_FLOAT_EQ(scale, 1.0f);
    EXPECT_FLOAT_EQ(bias, 0.0f);

    reasoning_calibration_destroy(cal);
}

TEST_F(ReasoningCalibrationTest, GetAdjustmentAfterRecords) {
    calibration_config_t config = reasoning_calibration_default_config();
    config.min_predictions_before_adjust = 3;
    reasoning_calibration_t* cal = reasoning_calibration_create(&config);
    ASSERT_NE(cal, nullptr);

    /* Record enough to trigger adjustment */
    for (int i = 0; i < 10; i++) {
        reasoning_calibration_record(cal, "test_module", 0.8f, 1.0f);
    }

    float scale = 0.0f, bias = 0.0f;
    int rc = reasoning_calibration_get_adjustment(
        cal, "test_module", &scale, &bias);
    EXPECT_EQ(rc, 0);
    /* Good predictor: scale should be near 1.0 */
    EXPECT_GT(scale, 0.5f);
    EXPECT_LE(scale, 2.0f);

    reasoning_calibration_destroy(cal);
}

TEST_F(ReasoningCalibrationTest, MinPredictionsBeforeAdjust) {
    calibration_config_t config = reasoning_calibration_default_config();
    config.min_predictions_before_adjust = 10;
    reasoning_calibration_t* cal = reasoning_calibration_create(&config);
    ASSERT_NE(cal, nullptr);

    /* Record only 5 predictions (below threshold) */
    for (int i = 0; i < 5; i++) {
        reasoning_calibration_record(cal, "test_module", 0.5f, 0.0f);
    }

    /* Scale should still be default 1.0 (not yet adjusted) */
    float scale = 0.0f, bias = 0.0f;
    reasoning_calibration_get_adjustment(cal, "test_module", &scale, &bias);
    EXPECT_FLOAT_EQ(scale, 1.0f);
    EXPECT_FLOAT_EQ(bias, 0.0f);

    reasoning_calibration_destroy(cal);
}

TEST_F(ReasoningCalibrationTest, ScaleClamping) {
    calibration_config_t config = reasoning_calibration_default_config();
    config.min_predictions_before_adjust = 1;
    config.min_scale = 0.2f;
    config.max_scale = 1.5f;
    config.learning_rate = 0.9f; /* Aggressive learning */
    reasoning_calibration_t* cal = reasoning_calibration_create(&config);
    ASSERT_NE(cal, nullptr);

    /* Terrible predictor: predict 0.9, actual always 0.0 */
    for (int i = 0; i < 20; i++) {
        reasoning_calibration_record(cal, "bad_module", 0.9f, 0.0f);
    }

    float scale = 0.0f, bias = 0.0f;
    reasoning_calibration_get_adjustment(cal, "bad_module", &scale, &bias);
    /* Scale should be clamped at min_scale */
    EXPECT_GE(scale, 0.2f);

    reasoning_calibration_destroy(cal);
}

TEST_F(ReasoningCalibrationTest, EMAErrorTracking) {
    calibration_config_t config = reasoning_calibration_default_config();
    config.learning_rate = 0.5f;  /* Fast adaptation */
    reasoning_calibration_t* cal = reasoning_calibration_create(&config);
    ASSERT_NE(cal, nullptr);

    /* Record perfect predictions */
    for (int i = 0; i < 10; i++) {
        reasoning_calibration_record(cal, "good_module", 0.8f, 1.0f);
    }

    contributor_calibration_t stats;
    reasoning_calibration_get_contributor_stats(cal, "good_module", &stats);
    /* Error should be low (predicted=0.8, actual=1.0, error=0.2) */
    EXPECT_LT(stats.ema_error, 0.5f);

    reasoning_calibration_destroy(cal);
}

TEST_F(ReasoningCalibrationTest, ReliabilityScore) {
    reasoning_calibration_t* cal = reasoning_calibration_create(NULL);
    ASSERT_NE(cal, nullptr);

    /* 8 correct, 2 wrong */
    for (int i = 0; i < 10; i++) {
        float outcome = (i < 8) ? 1.0f : 0.0f;
        reasoning_calibration_record(cal, "test_module", 0.5f, outcome);
    }

    contributor_calibration_t stats;
    reasoning_calibration_get_contributor_stats(cal, "test_module", &stats);
    EXPECT_NEAR(stats.reliability_score, 0.8f, 0.01f);

    reasoning_calibration_destroy(cal);
}

/*=============================================================================
 * STATS TESTS
 *===========================================================================*/

TEST_F(ReasoningCalibrationTest, GetContributorStats) {
    reasoning_calibration_t* cal = reasoning_calibration_create(NULL);
    ASSERT_NE(cal, nullptr);

    reasoning_calibration_record(cal, "hippocampus", 0.6f, 1.0f);
    reasoning_calibration_record(cal, "hippocampus", 0.7f, 0.0f);

    contributor_calibration_t stats;
    int rc = reasoning_calibration_get_contributor_stats(
        cal, "hippocampus", &stats);
    EXPECT_EQ(rc, 0);
    EXPECT_STREQ(stats.contributor_name, "hippocampus");
    EXPECT_EQ(stats.total_predictions, 2u);
    EXPECT_EQ(stats.correct_predictions, 1u);
    EXPECT_FLOAT_EQ(stats.reliability_score, 0.5f);

    reasoning_calibration_destroy(cal);
}

TEST_F(ReasoningCalibrationTest, GetContributorStatsUnknown) {
    reasoning_calibration_t* cal = reasoning_calibration_create(NULL);
    ASSERT_NE(cal, nullptr);

    contributor_calibration_t stats;
    int rc = reasoning_calibration_get_contributor_stats(
        cal, "nonexistent", &stats);
    EXPECT_EQ(rc, -1);

    reasoning_calibration_destroy(cal);
}

TEST_F(ReasoningCalibrationTest, GetGlobalStats) {
    reasoning_calibration_t* cal = reasoning_calibration_create(NULL);
    ASSERT_NE(cal, nullptr);

    /* Perfect predictor */
    for (int i = 0; i < 5; i++) {
        reasoning_calibration_record(cal, "good_one", 0.9f, 1.0f);
    }
    /* Terrible predictor */
    for (int i = 0; i < 5; i++) {
        reasoning_calibration_record(cal, "bad_one", 0.9f, 0.0f);
    }

    calibration_stats_t stats;
    int rc = reasoning_calibration_get_stats(cal, &stats);
    EXPECT_EQ(rc, 0);
    EXPECT_EQ(stats.total_records, 10u);
    EXPECT_EQ(stats.total_contributors_tracked, 2u);
    EXPECT_NEAR(stats.avg_reliability, 0.5f, 0.01f);

    reasoning_calibration_destroy(cal);
}

TEST_F(ReasoningCalibrationTest, BestWorstContributor) {
    reasoning_calibration_t* cal = reasoning_calibration_create(NULL);
    ASSERT_NE(cal, nullptr);

    /* Perfect predictor */
    for (int i = 0; i < 5; i++) {
        reasoning_calibration_record(cal, "best_module", 0.9f, 1.0f);
    }
    /* Terrible predictor */
    for (int i = 0; i < 5; i++) {
        reasoning_calibration_record(cal, "worst_module", 0.9f, 0.0f);
    }

    calibration_stats_t stats;
    reasoning_calibration_get_stats(cal, &stats);
    EXPECT_STREQ(stats.best_contributor_name, "best_module");
    EXPECT_STREQ(stats.worst_contributor_name, "worst_module");

    reasoning_calibration_destroy(cal);
}

/*=============================================================================
 * RESET TESTS
 *===========================================================================*/

TEST_F(ReasoningCalibrationTest, ResetClearsAllData) {
    reasoning_calibration_t* cal = reasoning_calibration_create(NULL);
    ASSERT_NE(cal, nullptr);

    /* Add some data */
    for (int i = 0; i < 10; i++) {
        reasoning_calibration_record(cal, "module_a", 0.5f, 1.0f);
    }
    reasoning_calibration_record(cal, "module_b", 0.8f, 0.0f);

    /* Reset */
    int rc = reasoning_calibration_reset(cal);
    EXPECT_EQ(rc, 0);

    /* Verify everything is cleared */
    calibration_stats_t stats;
    reasoning_calibration_get_stats(cal, &stats);
    EXPECT_EQ(stats.total_records, 0u);
    EXPECT_EQ(stats.total_contributors_tracked, 0u);

    /* Previously known contributor should now be unknown */
    contributor_calibration_t contrib_stats;
    rc = reasoning_calibration_get_contributor_stats(
        cal, "module_a", &contrib_stats);
    EXPECT_EQ(rc, -1);

    reasoning_calibration_destroy(cal);
}

/*=============================================================================
 * THREAD SAFETY TESTS
 *===========================================================================*/

TEST_F(ReasoningCalibrationTest, ThreadSafety) {
    reasoning_calibration_t* cal = reasoning_calibration_create(NULL);
    ASSERT_NE(cal, nullptr);

    const int num_threads = 4;
    const int records_per_thread = 50;

    auto worker = [&](int thread_id) {
        char name[64];
        snprintf(name, sizeof(name), "thread_%d", thread_id);
        for (int i = 0; i < records_per_thread; i++) {
            float outcome = (i % 2 == 0) ? 1.0f : 0.0f;
            reasoning_calibration_record(cal, name, 0.5f, outcome);
        }
    };

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back(worker, t);
    }
    for (auto& th : threads) {
        th.join();
    }

    /* Verify all records were counted */
    calibration_stats_t stats;
    reasoning_calibration_get_stats(cal, &stats);
    EXPECT_EQ(stats.total_records, (uint32_t)(num_threads * records_per_thread));
    EXPECT_EQ(stats.total_contributors_tracked, (uint32_t)num_threads);

    reasoning_calibration_destroy(cal);
}

/*=============================================================================
 * NULL INPUT TESTS
 *===========================================================================*/

TEST_F(ReasoningCalibrationTest, NullInputs) {
    reasoning_calibration_t* cal = reasoning_calibration_create(NULL);
    ASSERT_NE(cal, nullptr);

    /* Record with NULL cal */
    EXPECT_EQ(reasoning_calibration_record(NULL, "test", 0.5f, 1.0f), -1);
    /* Record with NULL name */
    EXPECT_EQ(reasoning_calibration_record(cal, NULL, 0.5f, 1.0f), -1);

    /* GetAdjustment with NULL cal */
    float s, b;
    EXPECT_EQ(reasoning_calibration_get_adjustment(NULL, "test", &s, &b), -1);
    /* GetAdjustment with NULL name */
    EXPECT_EQ(reasoning_calibration_get_adjustment(cal, NULL, &s, &b), -1);

    /* GetContributorStats with NULLs */
    contributor_calibration_t cs;
    EXPECT_EQ(reasoning_calibration_get_contributor_stats(NULL, "test", &cs), -1);
    EXPECT_EQ(reasoning_calibration_get_contributor_stats(cal, NULL, &cs), -1);
    EXPECT_EQ(reasoning_calibration_get_contributor_stats(cal, "test", NULL), -1);

    /* GetStats with NULLs */
    calibration_stats_t gs;
    EXPECT_EQ(reasoning_calibration_get_stats(NULL, &gs), -1);
    EXPECT_EQ(reasoning_calibration_get_stats(cal, NULL), -1);

    /* Reset with NULL */
    EXPECT_EQ(reasoning_calibration_reset(NULL), -1);

    reasoning_calibration_destroy(cal);
}

/*=============================================================================
 * PREDICTOR BEHAVIOR TESTS
 *===========================================================================*/

TEST_F(ReasoningCalibrationTest, PerfectPredictorScale) {
    calibration_config_t config = reasoning_calibration_default_config();
    config.min_predictions_before_adjust = 3;
    config.learning_rate = 0.1f;
    reasoning_calibration_t* cal = reasoning_calibration_create(&config);
    ASSERT_NE(cal, nullptr);

    /* Perfect predictor: always predicts correctly */
    for (int i = 0; i < 20; i++) {
        reasoning_calibration_record(cal, "perfect", 0.9f, 1.0f);
    }

    float scale = 0.0f, bias = 0.0f;
    reasoning_calibration_get_adjustment(cal, "perfect", &scale, &bias);
    /* Scale should stay near 1.0 (error is |0.9-1.0|=0.1, so scale ~ 0.9) */
    EXPECT_GT(scale, 0.7f);
    EXPECT_LE(scale, 1.5f);

    reasoning_calibration_destroy(cal);
}

TEST_F(ReasoningCalibrationTest, TerriblePredictorScale) {
    calibration_config_t config = reasoning_calibration_default_config();
    config.min_predictions_before_adjust = 2;
    config.learning_rate = 0.3f;  /* Faster adaptation */
    config.min_scale = 0.1f;
    reasoning_calibration_t* cal = reasoning_calibration_create(&config);
    ASSERT_NE(cal, nullptr);

    /* Terrible predictor: always wrong, high confidence in wrong answer */
    for (int i = 0; i < 30; i++) {
        reasoning_calibration_record(cal, "terrible", 0.95f, 0.0f);
    }

    float scale = 0.0f, bias = 0.0f;
    reasoning_calibration_get_adjustment(cal, "terrible", &scale, &bias);
    /* Scale should drop towards min_scale (error ~ 0.95) */
    EXPECT_LE(scale, 0.3f);
    EXPECT_GE(scale, 0.1f);

    contributor_calibration_t stats;
    reasoning_calibration_get_contributor_stats(cal, "terrible", &stats);
    EXPECT_FLOAT_EQ(stats.reliability_score, 0.0f);

    reasoning_calibration_destroy(cal);
}

TEST_F(ReasoningCalibrationTest, LearningRateAffectsSpeed) {
    /* Fast learner */
    calibration_config_t fast_config = reasoning_calibration_default_config();
    fast_config.learning_rate = 0.5f;
    fast_config.min_predictions_before_adjust = 1;
    reasoning_calibration_t* fast_cal = reasoning_calibration_create(&fast_config);
    ASSERT_NE(fast_cal, nullptr);

    /* Slow learner */
    calibration_config_t slow_config = reasoning_calibration_default_config();
    slow_config.learning_rate = 0.01f;
    slow_config.min_predictions_before_adjust = 1;
    reasoning_calibration_t* slow_cal = reasoning_calibration_create(&slow_config);
    ASSERT_NE(slow_cal, nullptr);

    /* Same bad predictions for both */
    for (int i = 0; i < 5; i++) {
        reasoning_calibration_record(fast_cal, "test", 0.9f, 0.0f);
        reasoning_calibration_record(slow_cal, "test", 0.9f, 0.0f);
    }

    contributor_calibration_t fast_stats, slow_stats;
    reasoning_calibration_get_contributor_stats(fast_cal, "test", &fast_stats);
    reasoning_calibration_get_contributor_stats(slow_cal, "test", &slow_stats);

    /* Fast learner should have adapted more (lower scale) */
    EXPECT_LT(fast_stats.confidence_scale, slow_stats.confidence_scale);

    reasoning_calibration_destroy(fast_cal);
    reasoning_calibration_destroy(slow_cal);
}

/*=============================================================================
 * ENGINE CONFIG INTEGRATION TESTS
 *===========================================================================*/

TEST_F(ReasoningCalibrationTest, EngineDefaultConfigCalibration) {
    reasoning_engine_config_t config = reasoning_engine_default_config();

    /* Calibration should be disabled by default */
    EXPECT_FALSE(config.enable_calibration);
    EXPECT_FLOAT_EQ(config.calibration_learning_rate,
                     REASONING_DEFAULT_CALIBRATION_LEARNING_RATE);
}
