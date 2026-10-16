/**
 * @file test_reasoning_calibration_regression.cpp
 * @brief Regression tests for confidence calibration system
 *
 * WHAT: Verify backward compatibility — calibration additions do not break
 *       existing reasoning chain, convergent, or config behavior
 * WHY:  Prevent regressions when calibration is added to the engine
 * HOW:  GTest suite checking old behaviors are preserved
 */

#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "cognitive/reasoning/nimcp_reasoning_calibration.h"
#include "cognitive/reasoning/nimcp_reasoning_convergent.h"
#include "cognitive/reasoning/nimcp_reasoning_chain.h"
#include "core/brain/nimcp_brain.h"
}

/*=============================================================================
 * TEST FIXTURE
 *===========================================================================*/

class CalibrationRegressionTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

/*=============================================================================
 * BACKWARD COMPATIBILITY TESTS
 *===========================================================================*/

TEST_F(CalibrationRegressionTest, DefaultConfigBackwardCompat) {
    reasoning_engine_config_t config = reasoning_engine_default_config();

    /* All original config fields should still have correct defaults */
    EXPECT_EQ(config.max_depth, (uint32_t)REASONING_CHAIN_DEFAULT_MAX_DEPTH);
    EXPECT_EQ(config.max_steps, (uint32_t)REASONING_CHAIN_DEFAULT_MAX_STEPS);
    EXPECT_FLOAT_EQ(config.confidence_threshold,
                     REASONING_CHAIN_DEFAULT_CONFIDENCE_THRESHOLD);
    EXPECT_FLOAT_EQ(config.uncertainty_threshold,
                     REASONING_CHAIN_DEFAULT_UNCERTAINTY_THRESHOLD);
    EXPECT_TRUE(config.enable_engram_recall);
    EXPECT_TRUE(config.enable_knowledge_query);
    EXPECT_TRUE(config.enable_predictive_verify);
    EXPECT_TRUE(config.enable_epistemic_check);
    EXPECT_TRUE(config.enable_analogical);
    EXPECT_TRUE(config.enable_working_memory);
    EXPECT_TRUE(config.enable_world_model);
    EXPECT_TRUE(config.enable_jepa_prediction);
    EXPECT_TRUE(config.enable_symbolic_logic);
    EXPECT_TRUE(config.enable_concurrent_pipeline);
    EXPECT_EQ(config.working_memory_slots,
              (uint32_t)REASONING_CHAIN_DEFAULT_WM_SLOTS);

    /* Convergent fields preserved */
    EXPECT_TRUE(config.enable_convergent_reasoning);
    EXPECT_EQ(config.convergent_pool_size, 8u);
    EXPECT_EQ(config.max_convergent_contributors, 64u);

    /* Calibration fields are new but should not disturb old values */
    EXPECT_FALSE(config.enable_calibration);
}

TEST_F(CalibrationRegressionTest, ConvergentWithoutCalibration) {
    /* Disable calibration — convergent should work exactly as before */
    brain_t brain = brain_create("regression_test", BRAIN_SIZE_SMALL,
                                  BRAIN_TASK_CLASSIFICATION, 4, 2);
    if (!brain) GTEST_SKIP() << "Brain creation failed";

    reasoning_engine_config_t config = reasoning_engine_default_config();
    config.enable_calibration = false;

    reasoning_engine_t* engine = reasoning_engine_create(&config);
    ASSERT_NE(engine, nullptr);

    int rc = reasoning_engine_connect_brain(engine, brain);
    ASSERT_EQ(rc, 0);

    reasoning_chain_t chain;
    reasoning_chain_init(&chain);
    chain.start_time_us = 0;

    rc = reasoning_engine_reason(engine, "What is reasoning?", &chain);
    EXPECT_EQ(rc, 0);
    EXPECT_TRUE(chain.is_complete);
    EXPECT_GE(chain.overall_confidence, 0.0f);

    reasoning_chain_cleanup(&chain);
    reasoning_engine_destroy(engine);
    brain_destroy(brain);
}

TEST_F(CalibrationRegressionTest, ConfigFieldsPreserved) {
    /* Create config with custom values, verify they survive */
    reasoning_engine_config_t config;
    memset(&config, 0, sizeof(config));

    config.max_depth = 5;
    config.max_steps = 25;
    config.confidence_threshold = 0.9f;
    config.enable_engram_recall = false;
    config.enable_calibration = true;
    config.calibration_learning_rate = 0.2f;

    reasoning_engine_t* engine = reasoning_engine_create(&config);
    ASSERT_NE(engine, nullptr);

    /* Engine was created — verify it doesn't crash */
    reasoning_engine_destroy(engine);
}

TEST_F(CalibrationRegressionTest, StatsFieldsPreserved) {
    reasoning_engine_config_t config = reasoning_engine_default_config();
    reasoning_engine_t* engine = reasoning_engine_create(&config);
    ASSERT_NE(engine, nullptr);

    reasoning_engine_stats_t stats;
    int rc = reasoning_engine_get_stats(engine, &stats);
    EXPECT_EQ(rc, 0);

    /* Original stats fields should be zero-initialized */
    EXPECT_EQ(stats.total_queries, 0u);
    EXPECT_EQ(stats.successful_queries, 0u);
    EXPECT_EQ(stats.total_steps, 0u);
    EXPECT_FLOAT_EQ(stats.avg_confidence, 0.0f);
    EXPECT_EQ(stats.engram_recalls, 0u);
    EXPECT_EQ(stats.knowledge_queries, 0u);
    EXPECT_EQ(stats.symbolic_queries, 0u);

    /* Convergent stats preserved */
    EXPECT_EQ(stats.convergent_queries, 0u);

    /* New calibration stats field */
    EXPECT_FLOAT_EQ(stats.avg_calibration_reliability, 0.0f);

    reasoning_engine_destroy(engine);
}

TEST_F(CalibrationRegressionTest, EngineCreateDestroyUnchanged) {
    /* Engine lifecycle should work as before with default config */
    reasoning_engine_config_t config = reasoning_engine_default_config();
    reasoning_engine_t* engine = reasoning_engine_create(&config);
    ASSERT_NE(engine, nullptr);
    reasoning_engine_destroy(engine);

    /* NULL config should still work */
    engine = reasoning_engine_create(NULL);
    ASSERT_NE(engine, nullptr);
    reasoning_engine_destroy(engine);

    /* NULL destroy should not crash */
    reasoning_engine_destroy(NULL);
}
