/**
 * @file test_reasoning_calibration_integration.cpp
 * @brief Integration tests for confidence calibration with reasoning engine
 *
 * WHAT: Tests calibration connected to reasoning engine and live brain
 * WHY:  Verify calibration integrates with convergent reasoning,
 *       Portia budget, and persists across queries
 * HOW:  GTest suite with brain create/destroy lifecycle
 */

#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "cognitive/reasoning/nimcp_reasoning_calibration.h"
#include "cognitive/reasoning/nimcp_reasoning_convergent.h"
#include "cognitive/reasoning/nimcp_reasoning_chain.h"
#include "cognitive/reasoning/nimcp_reasoning_portia_bridge.h"
#include "core/brain/nimcp_brain.h"
}

/*=============================================================================
 * TEST FIXTURE
 *===========================================================================*/

class CalibrationIntegrationTest : public ::testing::Test {
protected:
    brain_t brain = NULL;

    void SetUp() override {
        brain = brain_create("calibration_test", BRAIN_SIZE_SMALL,
                             BRAIN_TASK_CLASSIFICATION, 4, 2);
    }

    void TearDown() override {
        if (brain) {
            brain_destroy(brain);
            brain = NULL;
        }
    }
};

/*=============================================================================
 * INTEGRATION TESTS
 *===========================================================================*/

TEST_F(CalibrationIntegrationTest, CalibrationWithEngine) {
    if (!brain) GTEST_SKIP() << "Brain creation failed";

    /* Create engine with calibration enabled */
    reasoning_engine_config_t config = reasoning_engine_default_config();
    config.enable_calibration = true;
    config.calibration_learning_rate = 0.1f;

    reasoning_engine_t* engine = reasoning_engine_create(&config);
    ASSERT_NE(engine, nullptr);

    int rc = reasoning_engine_connect_brain(engine, brain);
    ASSERT_EQ(rc, 0);

    /* Run a query */
    reasoning_chain_t chain;
    reasoning_chain_init(&chain);
    chain.start_time_us = 0;

    rc = reasoning_engine_reason(engine, "What is learning?", &chain);
    EXPECT_EQ(rc, 0);
    EXPECT_TRUE(chain.is_complete);

    reasoning_chain_cleanup(&chain);
    reasoning_engine_destroy(engine);
}

TEST_F(CalibrationIntegrationTest, CalibrationAffectsConvergent) {
    if (!brain) GTEST_SKIP() << "Brain creation failed";

    /* Create standalone calibration system and seed it */
    calibration_config_t cal_config = reasoning_calibration_default_config();
    cal_config.min_predictions_before_adjust = 2;
    cal_config.learning_rate = 0.3f;
    reasoning_calibration_t* cal = reasoning_calibration_create(&cal_config);
    ASSERT_NE(cal, nullptr);

    /* Train calibration: hippocampus is unreliable */
    for (int i = 0; i < 10; i++) {
        reasoning_calibration_record(cal, "hippocampus", 0.8f, 0.0f);
    }

    /* Verify scale has dropped */
    float scale = 0.0f, bias = 0.0f;
    reasoning_calibration_get_adjustment(cal, "hippocampus", &scale, &bias);
    EXPECT_LT(scale, 0.5f);

    /* semantic_memory is unknown — should get default 1.0 */
    reasoning_calibration_get_adjustment(cal, "semantic_memory", &scale, &bias);
    EXPECT_FLOAT_EQ(scale, 1.0f);

    reasoning_calibration_destroy(cal);
}

TEST_F(CalibrationIntegrationTest, CalibrationWithBrain) {
    if (!brain) GTEST_SKIP() << "Brain creation failed";

    /* Full integration: brain + engine + calibration */
    reasoning_engine_config_t config = reasoning_engine_default_config();
    config.enable_calibration = true;

    reasoning_engine_t* engine = reasoning_engine_create(&config);
    ASSERT_NE(engine, nullptr);

    int rc = reasoning_engine_connect_brain(engine, brain);
    ASSERT_EQ(rc, 0);

    /* Run query */
    reasoning_chain_t chain;
    reasoning_chain_init(&chain);
    chain.start_time_us = 0;

    rc = reasoning_engine_reason(engine, "How does vision work?", &chain);
    EXPECT_EQ(rc, 0);
    EXPECT_GE(chain.overall_confidence, 0.0f);

    reasoning_chain_cleanup(&chain);
    reasoning_engine_destroy(engine);
}

TEST_F(CalibrationIntegrationTest, DisabledCalibrationNoEffect) {
    if (!brain) GTEST_SKIP() << "Brain creation failed";

    /* Calibration disabled — should work exactly as before */
    reasoning_engine_config_t config = reasoning_engine_default_config();
    config.enable_calibration = false;

    reasoning_engine_t* engine = reasoning_engine_create(&config);
    ASSERT_NE(engine, nullptr);

    int rc = reasoning_engine_connect_brain(engine, brain);
    ASSERT_EQ(rc, 0);

    reasoning_chain_t chain;
    reasoning_chain_init(&chain);
    chain.start_time_us = 0;

    rc = reasoning_engine_reason(engine, "What is memory?", &chain);
    EXPECT_EQ(rc, 0);
    EXPECT_TRUE(chain.is_complete);

    reasoning_chain_cleanup(&chain);
    reasoning_engine_destroy(engine);
}

TEST_F(CalibrationIntegrationTest, CalibrationPersistsAcrossQueries) {
    /* Calibration state should be maintained between reasoning calls */
    reasoning_calibration_t* cal = reasoning_calibration_create(NULL);
    ASSERT_NE(cal, nullptr);

    /* First "session" of records */
    reasoning_calibration_record(cal, "hippocampus", 0.7f, 1.0f);
    reasoning_calibration_record(cal, "hippocampus", 0.6f, 0.0f);

    calibration_stats_t stats;
    reasoning_calibration_get_stats(cal, &stats);
    EXPECT_EQ(stats.total_records, 2u);

    /* Second "session" of records — state should persist */
    reasoning_calibration_record(cal, "hippocampus", 0.8f, 1.0f);
    reasoning_calibration_record(cal, "parietal", 0.5f, 1.0f);

    reasoning_calibration_get_stats(cal, &stats);
    EXPECT_EQ(stats.total_records, 4u);
    EXPECT_EQ(stats.total_contributors_tracked, 2u);

    contributor_calibration_t hip_stats;
    reasoning_calibration_get_contributor_stats(cal, "hippocampus", &hip_stats);
    EXPECT_EQ(hip_stats.total_predictions, 3u);

    reasoning_calibration_destroy(cal);
}

TEST_F(CalibrationIntegrationTest, CalibrationStatsPopulated) {
    reasoning_calibration_t* cal = reasoning_calibration_create(NULL);
    ASSERT_NE(cal, nullptr);

    /* Record data for multiple contributors */
    for (int i = 0; i < 10; i++) {
        reasoning_calibration_record(cal, "good_module", 0.8f, 1.0f);
        reasoning_calibration_record(cal, "bad_module", 0.9f, 0.0f);
        reasoning_calibration_record(cal, "medium_module", 0.5f,
                                     (i % 2 == 0) ? 1.0f : 0.0f);
    }

    calibration_stats_t stats;
    int rc = reasoning_calibration_get_stats(cal, &stats);
    EXPECT_EQ(rc, 0);
    EXPECT_EQ(stats.total_records, 30u);
    EXPECT_EQ(stats.total_contributors_tracked, 3u);
    EXPECT_GT(stats.avg_reliability, 0.0f);
    EXPECT_LT(stats.avg_reliability, 1.0f);
    EXPECT_GT(strlen(stats.best_contributor_name), 0u);
    EXPECT_GT(strlen(stats.worst_contributor_name), 0u);

    reasoning_calibration_destroy(cal);
}

TEST_F(CalibrationIntegrationTest, CalibrationWithPortiaBudget) {
    if (!brain) GTEST_SKIP() << "Brain creation failed";

    /* Calibration should work under Portia resource constraints */
    reasoning_engine_config_t config = reasoning_engine_default_config();
    config.enable_calibration = true;

    /* Apply a budget that reduces contributors */
    reasoning_budget_t budget = reasoning_portia_full_budget();
    budget.max_convergent_contributors = 8;
    reasoning_portia_apply_budget(&config, &budget);

    reasoning_engine_t* engine = reasoning_engine_create(&config);
    ASSERT_NE(engine, nullptr);

    int rc = reasoning_engine_connect_brain(engine, brain);
    ASSERT_EQ(rc, 0);

    reasoning_chain_t chain;
    reasoning_chain_init(&chain);
    chain.start_time_us = 0;

    rc = reasoning_engine_reason(engine, "Is this resource constrained?", &chain);
    EXPECT_EQ(rc, 0);

    reasoning_chain_cleanup(&chain);
    reasoning_engine_destroy(engine);
}
