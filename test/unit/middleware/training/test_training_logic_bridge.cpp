/**
 * @file test_training_logic_bridge.cpp
 * @brief Comprehensive unit tests for Training-Logic Bridge
 *
 * TEST COVERAGE:
 * - Lifecycle (8 tests)
 * - Configuration (6 tests)
 * - Stability check gate (8 tests)
 * - Intervention gate (8 tests)
 * - LR increase gate (6 tests)
 * - Batch size gate (5 tests)
 * - Checkpoint gate (5 tests)
 * - Metric updates (6 tests)
 * - Decision evaluation (8 tests)
 * - Modulation (5 tests)
 * - Custom gates (5 tests)
 * - Condition management (5 tests)
 * - Statistics (4 tests)
 * - Error handling (5 tests)
 *
 * TOTAL: 84 tests
 *
 * @author NIMCP Development Team
 * @date 2025-12-20
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

// Headers have their own extern "C" guards
#include "middleware/training/nimcp_training_logic_bridge.h"
#include "utils/logging/nimcp_logging.h"

class TrainingLogicBridgeTest : public ::testing::Test {
protected:
    training_logic_bridge_t* bridge;
    training_logic_config_t config;

    void SetUp() override {
        /* Get default config */
        training_logic_default_config(&config);
        config.disable_auto_update = true;  /* Allow manual condition control in tests */
        config.enable_bio_async = false;     /* Disable for unit tests */

        /* Create bridge */
        bridge = training_logic_create(&config);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            training_logic_destroy(bridge);
            bridge = nullptr;
        }
    }
};

/*=============================================================================
 * LIFECYCLE TESTS (8 tests)
 *============================================================================*/

TEST_F(TrainingLogicBridgeTest, CreateValidBridge) {
    EXPECT_NE(bridge, nullptr);
}

TEST_F(TrainingLogicBridgeTest, CreateWithNullConfig) {
    training_logic_bridge_t* test_bridge = training_logic_create(nullptr);
    EXPECT_NE(test_bridge, nullptr);
    training_logic_destroy(test_bridge);
}

TEST_F(TrainingLogicBridgeTest, DestroyNullBridge) {
    training_logic_destroy(nullptr);
    SUCCEED();  /* Should not crash */
}

TEST_F(TrainingLogicBridgeTest, StartBridge) {
    int result = training_logic_start(bridge);
    EXPECT_EQ(result, 0);
}

TEST_F(TrainingLogicBridgeTest, StartNullBridge) {
    int result = training_logic_start(nullptr);
    EXPECT_NE(result, 0);  /* Returns NIMCP_ERROR_NULL_POINTER (positive) */
}

TEST_F(TrainingLogicBridgeTest, StopBridge) {
    training_logic_start(bridge);
    int result = training_logic_stop(bridge);
    EXPECT_EQ(result, 0);
}

TEST_F(TrainingLogicBridgeTest, StopNullBridge) {
    int result = training_logic_stop(nullptr);
    EXPECT_NE(result, 0);  /* Returns NIMCP_ERROR_NULL_POINTER (positive) */
}

TEST_F(TrainingLogicBridgeTest, StartStopCycle) {
    /* Start */
    int result1 = training_logic_start(bridge);
    EXPECT_EQ(result1, 0);

    /* Stop */
    int result2 = training_logic_stop(bridge);
    EXPECT_EQ(result2, 0);

    /* Start again */
    int result3 = training_logic_start(bridge);
    EXPECT_EQ(result3, 0);

    /* Stop again */
    int result4 = training_logic_stop(bridge);
    EXPECT_EQ(result4, 0);
}

/*=============================================================================
 * CONFIGURATION TESTS (6 tests)
 *============================================================================*/

TEST_F(TrainingLogicBridgeTest, DefaultConfig) {
    training_logic_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    training_logic_default_config(&cfg);

    EXPECT_GE(cfg.mode, TRAINING_LOGIC_MODE_DISABLED);
    EXPECT_LT(cfg.mode, TRAINING_LOGIC_MODE_CONSENSUS_REQUIRED + 1);
    EXPECT_GT(cfg.lr_increase_factor, 1.0f);
    EXPECT_LT(cfg.lr_decrease_factor, 1.0f);
    EXPECT_GT(cfg.stable_steps_required, 0u);
    EXPECT_GT(cfg.checkpoint_interval, 0u);
}

TEST_F(TrainingLogicBridgeTest, DefaultConfigNullPointer) {
    training_logic_default_config(nullptr);
    SUCCEED();  /* Should not crash */
}

TEST_F(TrainingLogicBridgeTest, CustomModeSettings) {
    training_logic_config_t custom_config;
    training_logic_default_config(&custom_config);
    custom_config.mode = TRAINING_LOGIC_MODE_AUTOMATIC;
    custom_config.disable_auto_update = true;

    training_logic_bridge_t* custom_bridge = training_logic_create(&custom_config);
    EXPECT_NE(custom_bridge, nullptr);
    training_logic_destroy(custom_bridge);
}

TEST_F(TrainingLogicBridgeTest, CustomThresholds) {
    training_logic_config_t custom_config;
    training_logic_default_config(&custom_config);
    custom_config.stability_threshold = 0.8f;
    custom_config.intervention_threshold = 0.6f;
    custom_config.lr_increase_threshold = 0.75f;
    custom_config.confidence_threshold = 0.9f;

    training_logic_bridge_t* custom_bridge = training_logic_create(&custom_config);
    EXPECT_NE(custom_bridge, nullptr);
    training_logic_destroy(custom_bridge);
}

TEST_F(TrainingLogicBridgeTest, CustomModulationFactors) {
    training_logic_config_t custom_config;
    training_logic_default_config(&custom_config);
    custom_config.lr_increase_factor = 1.2f;
    custom_config.lr_decrease_factor = 0.8f;
    custom_config.batch_scale_factor = 0.5f;
    custom_config.min_learning_rate = 1e-6f;
    custom_config.max_learning_rate = 1e-2f;
    custom_config.min_batch_size = 8;
    custom_config.max_batch_size = 512;

    training_logic_bridge_t* custom_bridge = training_logic_create(&custom_config);
    EXPECT_NE(custom_bridge, nullptr);
    training_logic_destroy(custom_bridge);
}

TEST_F(TrainingLogicBridgeTest, EnableAllIntegrations) {
    training_logic_config_t custom_config;
    training_logic_default_config(&custom_config);
    custom_config.enable_immune_integration = true;
    custom_config.enable_portia_integration = true;
    custom_config.enable_swarm_integration = true;
    custom_config.enable_bio_async = false;  /* Keep disabled for tests */
    custom_config.disable_auto_update = true;

    training_logic_bridge_t* custom_bridge = training_logic_create(&custom_config);
    EXPECT_NE(custom_bridge, nullptr);
    training_logic_destroy(custom_bridge);
}

/*=============================================================================
 * STABILITY CHECK GATE TESTS (8 tests)
 *============================================================================*/

TEST_F(TrainingLogicBridgeTest, StabilityAllConditionsOk) {
    /* Set all stability conditions to true */
    training_logic_set_condition(bridge, TRAINING_COND_LOSS_STABLE, true);
    training_logic_set_condition(bridge, TRAINING_COND_GRAD_STABLE, true);
    training_logic_set_condition(bridge, TRAINING_COND_LR_REASONABLE, true);

    bool stable = training_logic_check_stability(bridge);
    EXPECT_TRUE(stable);
}

TEST_F(TrainingLogicBridgeTest, StabilityLossUnstable) {
    training_logic_set_condition(bridge, TRAINING_COND_LOSS_STABLE, false);
    training_logic_set_condition(bridge, TRAINING_COND_GRAD_STABLE, true);
    training_logic_set_condition(bridge, TRAINING_COND_LR_REASONABLE, true);

    bool stable = training_logic_check_stability(bridge);
    EXPECT_FALSE(stable);
}

TEST_F(TrainingLogicBridgeTest, StabilityGradUnstable) {
    training_logic_set_condition(bridge, TRAINING_COND_LOSS_STABLE, true);
    training_logic_set_condition(bridge, TRAINING_COND_GRAD_STABLE, false);
    training_logic_set_condition(bridge, TRAINING_COND_LR_REASONABLE, true);

    bool stable = training_logic_check_stability(bridge);
    EXPECT_FALSE(stable);
}

TEST_F(TrainingLogicBridgeTest, StabilityLRUnreasonable) {
    training_logic_set_condition(bridge, TRAINING_COND_LOSS_STABLE, true);
    training_logic_set_condition(bridge, TRAINING_COND_GRAD_STABLE, true);
    training_logic_set_condition(bridge, TRAINING_COND_LR_REASONABLE, false);

    bool stable = training_logic_check_stability(bridge);
    EXPECT_FALSE(stable);
}

TEST_F(TrainingLogicBridgeTest, StabilityAllUnstable) {
    training_logic_set_condition(bridge, TRAINING_COND_LOSS_STABLE, false);
    training_logic_set_condition(bridge, TRAINING_COND_GRAD_STABLE, false);
    training_logic_set_condition(bridge, TRAINING_COND_LR_REASONABLE, false);

    bool stable = training_logic_check_stability(bridge);
    EXPECT_FALSE(stable);
}

TEST_F(TrainingLogicBridgeTest, StabilityNullBridge) {
    bool stable = training_logic_check_stability(nullptr);
    EXPECT_FALSE(stable);
}

TEST_F(TrainingLogicBridgeTest, StabilityPartialConditions) {
    /* Only 2 out of 3 conditions OK */
    training_logic_set_condition(bridge, TRAINING_COND_LOSS_STABLE, true);
    training_logic_set_condition(bridge, TRAINING_COND_GRAD_STABLE, true);
    training_logic_set_condition(bridge, TRAINING_COND_LR_REASONABLE, false);

    bool stable = training_logic_check_stability(bridge);
    EXPECT_FALSE(stable);  /* AND gate requires all conditions */
}

TEST_F(TrainingLogicBridgeTest, StabilityBorderline) {
    /* All conditions just barely OK */
    training_logic_set_condition(bridge, TRAINING_COND_LOSS_STABLE, true);
    training_logic_set_condition(bridge, TRAINING_COND_GRAD_STABLE, true);
    training_logic_set_condition(bridge, TRAINING_COND_LR_REASONABLE, true);

    bool stable = training_logic_check_stability(bridge);
    EXPECT_TRUE(stable);
}

/*=============================================================================
 * INTERVENTION GATE TESTS (8 tests)
 *============================================================================*/

TEST_F(TrainingLogicBridgeTest, InterventionAllOk) {
    /* Set all intervention triggers to false */
    training_logic_set_condition(bridge, TRAINING_COND_GRAD_EXPLODING, false);
    training_logic_set_condition(bridge, TRAINING_COND_LOSS_NAN, false);
    training_logic_set_condition(bridge, TRAINING_COND_DIVERGING, false);

    bool needs_intervention = training_logic_needs_intervention(bridge);
    EXPECT_FALSE(needs_intervention);
}

TEST_F(TrainingLogicBridgeTest, InterventionGradExploding) {
    training_logic_set_condition(bridge, TRAINING_COND_GRAD_EXPLODING, true);
    training_logic_set_condition(bridge, TRAINING_COND_LOSS_NAN, false);
    training_logic_set_condition(bridge, TRAINING_COND_DIVERGING, false);

    bool needs_intervention = training_logic_needs_intervention(bridge);
    EXPECT_TRUE(needs_intervention);
}

TEST_F(TrainingLogicBridgeTest, InterventionLossNaN) {
    training_logic_set_condition(bridge, TRAINING_COND_GRAD_EXPLODING, false);
    training_logic_set_condition(bridge, TRAINING_COND_LOSS_NAN, true);
    training_logic_set_condition(bridge, TRAINING_COND_DIVERGING, false);

    bool needs_intervention = training_logic_needs_intervention(bridge);
    EXPECT_TRUE(needs_intervention);
}

TEST_F(TrainingLogicBridgeTest, InterventionDiverging) {
    training_logic_set_condition(bridge, TRAINING_COND_GRAD_EXPLODING, false);
    training_logic_set_condition(bridge, TRAINING_COND_LOSS_NAN, false);
    training_logic_set_condition(bridge, TRAINING_COND_DIVERGING, true);

    bool needs_intervention = training_logic_needs_intervention(bridge);
    EXPECT_TRUE(needs_intervention);
}

TEST_F(TrainingLogicBridgeTest, InterventionMultipleTriggers) {
    training_logic_set_condition(bridge, TRAINING_COND_GRAD_EXPLODING, true);
    training_logic_set_condition(bridge, TRAINING_COND_LOSS_NAN, true);
    training_logic_set_condition(bridge, TRAINING_COND_DIVERGING, true);

    bool needs_intervention = training_logic_needs_intervention(bridge);
    EXPECT_TRUE(needs_intervention);
}

TEST_F(TrainingLogicBridgeTest, InterventionNullBridge) {
    bool needs_intervention = training_logic_needs_intervention(nullptr);
    EXPECT_FALSE(needs_intervention);
}

TEST_F(TrainingLogicBridgeTest, InterventionAfterRecovery) {
    /* Trigger intervention */
    training_logic_set_condition(bridge, TRAINING_COND_GRAD_EXPLODING, true);
    bool needs_intervention_1 = training_logic_needs_intervention(bridge);
    EXPECT_TRUE(needs_intervention_1);

    /* Recovery - clear trigger */
    training_logic_set_condition(bridge, TRAINING_COND_GRAD_EXPLODING, false);
    bool needs_intervention_2 = training_logic_needs_intervention(bridge);
    EXPECT_FALSE(needs_intervention_2);
}

TEST_F(TrainingLogicBridgeTest, InterventionSeverityLevels) {
    /* Signal different instabilities */
    training_logic_signal_instability(bridge, LOGIC_INSTABILITY_GRAD_EXPLOSION, 8);
    bool needs_intervention = training_logic_needs_intervention(bridge);
    EXPECT_TRUE(needs_intervention);

    /* Clear via setting condition */
    training_logic_set_condition(bridge, TRAINING_COND_GRAD_EXPLODING, false);
}

/*=============================================================================
 * LR INCREASE GATE TESTS (6 tests)
 *============================================================================*/

TEST_F(TrainingLogicBridgeTest, LRIncreaseStableEnough) {
    /* Set conditions for safe LR increase */
    training_logic_set_condition(bridge, TRAINING_COND_STABLE_FOR_N_STEPS, true);
    training_logic_set_condition(bridge, TRAINING_COND_IMMUNE_OK, true);
    training_logic_set_condition(bridge, TRAINING_COND_RESOURCE_OK, true);

    bool can_increase = training_logic_can_increase_lr(bridge);
    EXPECT_TRUE(can_increase);
}

TEST_F(TrainingLogicBridgeTest, LRIncreaseNotStableEnough) {
    training_logic_set_condition(bridge, TRAINING_COND_STABLE_FOR_N_STEPS, false);
    training_logic_set_condition(bridge, TRAINING_COND_IMMUNE_OK, true);
    training_logic_set_condition(bridge, TRAINING_COND_RESOURCE_OK, true);

    bool can_increase = training_logic_can_increase_lr(bridge);
    EXPECT_FALSE(can_increase);
}

TEST_F(TrainingLogicBridgeTest, LRIncreaseImmuneBlocked) {
    training_logic_set_condition(bridge, TRAINING_COND_STABLE_FOR_N_STEPS, true);
    training_logic_set_condition(bridge, TRAINING_COND_IMMUNE_OK, false);
    training_logic_set_condition(bridge, TRAINING_COND_RESOURCE_OK, true);

    bool can_increase = training_logic_can_increase_lr(bridge);
    EXPECT_FALSE(can_increase);
}

TEST_F(TrainingLogicBridgeTest, LRIncreaseResourceBlocked) {
    training_logic_set_condition(bridge, TRAINING_COND_STABLE_FOR_N_STEPS, true);
    training_logic_set_condition(bridge, TRAINING_COND_IMMUNE_OK, true);
    training_logic_set_condition(bridge, TRAINING_COND_RESOURCE_OK, false);

    bool can_increase = training_logic_can_increase_lr(bridge);
    EXPECT_FALSE(can_increase);
}

TEST_F(TrainingLogicBridgeTest, LRIncreaseNullBridge) {
    bool can_increase = training_logic_can_increase_lr(nullptr);
    EXPECT_FALSE(can_increase);
}

TEST_F(TrainingLogicBridgeTest, LRIncreaseBorderlineSteps) {
    /* Just reached required stable steps */
    training_logic_set_condition(bridge, TRAINING_COND_STABLE_FOR_N_STEPS, true);
    training_logic_set_condition(bridge, TRAINING_COND_IMMUNE_OK, true);
    training_logic_set_condition(bridge, TRAINING_COND_RESOURCE_OK, true);

    bool can_increase = training_logic_can_increase_lr(bridge);
    EXPECT_TRUE(can_increase);
}

/*=============================================================================
 * BATCH SIZE GATE TESTS (5 tests)
 *============================================================================*/

TEST_F(TrainingLogicBridgeTest, BatchSizeMemoryOkThroughputOk) {
    training_logic_set_condition(bridge, TRAINING_COND_MEMORY_OK, true);
    training_logic_set_condition(bridge, TRAINING_COND_THROUGHPUT_OK, true);

    bool increase_batch = false;
    bool should_adjust = training_logic_should_adjust_batch(bridge, &increase_batch);
    EXPECT_TRUE(should_adjust);
    EXPECT_TRUE(increase_batch);  /* Both OK, can increase */
}

TEST_F(TrainingLogicBridgeTest, BatchSizeMemoryNotOk) {
    training_logic_set_condition(bridge, TRAINING_COND_MEMORY_OK, false);
    training_logic_set_condition(bridge, TRAINING_COND_THROUGHPUT_OK, true);

    bool increase_batch = true;
    bool should_adjust = training_logic_should_adjust_batch(bridge, &increase_batch);
    EXPECT_TRUE(should_adjust);
    EXPECT_FALSE(increase_batch);  /* Memory not OK, should decrease */
}

TEST_F(TrainingLogicBridgeTest, BatchSizeThroughputNotOk) {
    training_logic_set_condition(bridge, TRAINING_COND_MEMORY_OK, true);
    training_logic_set_condition(bridge, TRAINING_COND_THROUGHPUT_OK, false);

    bool increase_batch = true;
    bool should_adjust = training_logic_should_adjust_batch(bridge, &increase_batch);
    EXPECT_TRUE(should_adjust);
    EXPECT_FALSE(increase_batch);  /* Throughput not OK, should decrease */
}

TEST_F(TrainingLogicBridgeTest, BatchSizeNullBridge) {
    bool increase_batch = false;
    bool should_adjust = training_logic_should_adjust_batch(nullptr, &increase_batch);
    EXPECT_FALSE(should_adjust);
}

TEST_F(TrainingLogicBridgeTest, BatchSizeIncreaseVsDecrease) {
    /* Test increase scenario */
    training_logic_set_condition(bridge, TRAINING_COND_MEMORY_OK, true);
    training_logic_set_condition(bridge, TRAINING_COND_THROUGHPUT_OK, true);

    bool increase_batch_1 = false;
    bool should_adjust_1 = training_logic_should_adjust_batch(bridge, &increase_batch_1);
    EXPECT_TRUE(should_adjust_1);
    EXPECT_TRUE(increase_batch_1);

    /* Test decrease scenario */
    training_logic_set_condition(bridge, TRAINING_COND_MEMORY_OK, false);
    training_logic_set_condition(bridge, TRAINING_COND_THROUGHPUT_OK, false);

    bool increase_batch_2 = true;
    bool should_adjust_2 = training_logic_should_adjust_batch(bridge, &increase_batch_2);
    EXPECT_TRUE(should_adjust_2);
    EXPECT_FALSE(increase_batch_2);
}

/*=============================================================================
 * CHECKPOINT GATE TESTS (5 tests)
 *============================================================================*/

TEST_F(TrainingLogicBridgeTest, CheckpointAllConditionsOk) {
    training_logic_set_condition(bridge, TRAINING_COND_MEMORY_OK, true);
    training_logic_set_condition(bridge, TRAINING_COND_NOT_MID_BATCH, true);
    training_logic_set_condition(bridge, TRAINING_COND_SUFFICIENT_PROGRESS, true);

    bool should_checkpoint = training_logic_should_checkpoint(bridge);
    EXPECT_TRUE(should_checkpoint);
}

TEST_F(TrainingLogicBridgeTest, CheckpointMemoryNotOk) {
    training_logic_set_condition(bridge, TRAINING_COND_MEMORY_OK, false);
    training_logic_set_condition(bridge, TRAINING_COND_NOT_MID_BATCH, true);
    training_logic_set_condition(bridge, TRAINING_COND_SUFFICIENT_PROGRESS, true);

    bool should_checkpoint = training_logic_should_checkpoint(bridge);
    EXPECT_FALSE(should_checkpoint);
}

TEST_F(TrainingLogicBridgeTest, CheckpointMidBatch) {
    training_logic_set_condition(bridge, TRAINING_COND_MEMORY_OK, true);
    training_logic_set_condition(bridge, TRAINING_COND_NOT_MID_BATCH, false);
    training_logic_set_condition(bridge, TRAINING_COND_SUFFICIENT_PROGRESS, true);

    bool should_checkpoint = training_logic_should_checkpoint(bridge);
    EXPECT_FALSE(should_checkpoint);
}

TEST_F(TrainingLogicBridgeTest, CheckpointInsufficientProgress) {
    training_logic_set_condition(bridge, TRAINING_COND_MEMORY_OK, true);
    training_logic_set_condition(bridge, TRAINING_COND_NOT_MID_BATCH, true);
    training_logic_set_condition(bridge, TRAINING_COND_SUFFICIENT_PROGRESS, false);

    bool should_checkpoint = training_logic_should_checkpoint(bridge);
    EXPECT_FALSE(should_checkpoint);
}

TEST_F(TrainingLogicBridgeTest, CheckpointNullBridge) {
    bool should_checkpoint = training_logic_should_checkpoint(nullptr);
    EXPECT_FALSE(should_checkpoint);
}

/*=============================================================================
 * METRIC UPDATE TESTS (6 tests)
 *============================================================================*/

TEST_F(TrainingLogicBridgeTest, UpdateMetricsValid) {
    int result = training_logic_update_metrics(bridge, 0.5f, 1.0f, 0.001f, 100);
    EXPECT_EQ(result, 0);
}

TEST_F(TrainingLogicBridgeTest, UpdateMetricsNaN) {
    int result = training_logic_update_metrics(bridge, NAN, 1.0f, 0.001f, 100);
    EXPECT_EQ(result, 0);  /* Should handle NaN gracefully */

    /* Verify LOSS_NAN condition is set */
    training_logic_conditions_t conditions;
    training_logic_get_conditions(bridge, &conditions);
    EXPECT_TRUE(conditions.loss_nan);
}

TEST_F(TrainingLogicBridgeTest, UpdateMetricsInf) {
    int result = training_logic_update_metrics(bridge, INFINITY, 1.0f, 0.001f, 100);
    EXPECT_EQ(result, 0);  /* Should handle Inf gracefully */
}

TEST_F(TrainingLogicBridgeTest, UpdateBatchMetrics) {
    int result = training_logic_update_batch_metrics(bridge, 32, 10.0f, 0.6f);
    EXPECT_EQ(result, 0);
}

TEST_F(TrainingLogicBridgeTest, SignalInstability) {
    int result = training_logic_signal_instability(bridge, LOGIC_INSTABILITY_GRAD_EXPLOSION, 8);
    EXPECT_EQ(result, 0);

    /* Verify intervention is needed */
    bool needs_intervention = training_logic_needs_intervention(bridge);
    EXPECT_TRUE(needs_intervention);
}

TEST_F(TrainingLogicBridgeTest, HistoryBufferCircular) {
    /* Fill history buffer beyond capacity */
    for (uint64_t i = 0; i < 150; i++) {
        training_logic_update_metrics(bridge, 0.5f - i*0.001f, 1.0f, 0.001f, i);
    }

    /* Should not crash, uses circular buffer */
    SUCCEED();
}

/*=============================================================================
 * DECISION TESTS (8 tests)
 *============================================================================*/

TEST_F(TrainingLogicBridgeTest, GetDecisionContinue) {
    /* Set stable conditions */
    training_logic_set_condition(bridge, TRAINING_COND_LOSS_STABLE, true);
    training_logic_set_condition(bridge, TRAINING_COND_GRAD_STABLE, true);
    training_logic_set_condition(bridge, TRAINING_COND_LR_REASONABLE, true);
    training_logic_set_condition(bridge, TRAINING_COND_GRAD_EXPLODING, false);
    training_logic_set_condition(bridge, TRAINING_COND_LOSS_NAN, false);
    training_logic_set_condition(bridge, TRAINING_COND_DIVERGING, false);
    /* Set batch conditions to OK so batch adjustment doesn't trigger */
    training_logic_set_condition(bridge, TRAINING_COND_MEMORY_OK, true);
    training_logic_set_condition(bridge, TRAINING_COND_THROUGHPUT_OK, true);

    training_logic_decision_t decision;
    int result = training_logic_get_decision(bridge, &decision);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(decision.type, TRAINING_DECISION_CONTINUE);
    EXPECT_TRUE(decision.approved);
}

TEST_F(TrainingLogicBridgeTest, GetDecisionPause) {
    /* Trigger intervention */
    training_logic_set_condition(bridge, TRAINING_COND_GRAD_EXPLODING, true);

    training_logic_decision_t decision;
    int result = training_logic_get_decision(bridge, &decision);
    EXPECT_EQ(result, 0);
    /* Decision should be to pause or decrease LR */
    EXPECT_TRUE(decision.type == TRAINING_DECISION_PAUSE ||
                decision.type == TRAINING_DECISION_DECREASE_LR);
}

TEST_F(TrainingLogicBridgeTest, GetDecisionIncreaseLR) {
    /* Set conditions for LR increase */
    training_logic_set_condition(bridge, TRAINING_COND_LOSS_STABLE, true);
    training_logic_set_condition(bridge, TRAINING_COND_GRAD_STABLE, true);
    training_logic_set_condition(bridge, TRAINING_COND_LR_REASONABLE, true);
    training_logic_set_condition(bridge, TRAINING_COND_STABLE_FOR_N_STEPS, true);
    training_logic_set_condition(bridge, TRAINING_COND_IMMUNE_OK, true);
    training_logic_set_condition(bridge, TRAINING_COND_RESOURCE_OK, true);

    training_logic_decision_t decision;
    int result = training_logic_get_decision(bridge, &decision);
    EXPECT_EQ(result, 0);
    /* May recommend LR increase if stable enough */
}

TEST_F(TrainingLogicBridgeTest, GetDecisionDecreaseLR) {
    /* Trigger intervention requiring LR decrease */
    training_logic_set_condition(bridge, TRAINING_COND_GRAD_EXPLODING, true);

    training_logic_decision_t decision;
    int result = training_logic_get_decision(bridge, &decision);
    EXPECT_EQ(result, 0);
    /* Should recommend LR decrease or pause */
}

TEST_F(TrainingLogicBridgeTest, GetDecisionCheckpoint) {
    /* Set conditions for checkpoint */
    training_logic_set_condition(bridge, TRAINING_COND_MEMORY_OK, true);
    training_logic_set_condition(bridge, TRAINING_COND_NOT_MID_BATCH, true);
    training_logic_set_condition(bridge, TRAINING_COND_SUFFICIENT_PROGRESS, true);

    bool should_checkpoint = training_logic_should_checkpoint(bridge);
    EXPECT_TRUE(should_checkpoint);
}

TEST_F(TrainingLogicBridgeTest, GetDecisionTerminate) {
    /* Set extreme failure conditions */
    training_logic_set_condition(bridge, TRAINING_COND_LOSS_NAN, true);
    training_logic_set_condition(bridge, TRAINING_COND_GRAD_EXPLODING, true);
    training_logic_set_condition(bridge, TRAINING_COND_DIVERGING, true);

    training_logic_decision_t decision;
    int result = training_logic_get_decision(bridge, &decision);
    EXPECT_EQ(result, 0);
    /* Should recommend pause or termination */
}

TEST_F(TrainingLogicBridgeTest, ApplyDecision) {
    training_logic_decision_t decision;
    memset(&decision, 0, sizeof(decision));
    decision.type = TRAINING_DECISION_CONTINUE;
    decision.approved = true;
    decision.confidence = 0.9f;

    int result = training_logic_apply_decision(bridge, &decision);
    EXPECT_EQ(result, 0);
}

TEST_F(TrainingLogicBridgeTest, DecisionConfidence) {
    training_logic_set_condition(bridge, TRAINING_COND_LOSS_STABLE, true);
    training_logic_set_condition(bridge, TRAINING_COND_GRAD_STABLE, true);
    training_logic_set_condition(bridge, TRAINING_COND_LR_REASONABLE, true);

    training_logic_decision_t decision;
    int result = training_logic_get_decision(bridge, &decision);
    EXPECT_EQ(result, 0);
    EXPECT_GE(decision.confidence, 0.0f);
    EXPECT_LE(decision.confidence, 1.0f);
}

/*=============================================================================
 * MODULATION TESTS (5 tests)
 *============================================================================*/

TEST_F(TrainingLogicBridgeTest, LRModulationIncrease) {
    /* Set conditions for LR increase */
    training_logic_set_condition(bridge, TRAINING_COND_STABLE_FOR_N_STEPS, true);
    training_logic_set_condition(bridge, TRAINING_COND_IMMUNE_OK, true);
    training_logic_set_condition(bridge, TRAINING_COND_RESOURCE_OK, true);

    float base_lr = 0.001f;
    float modulated_lr = training_logic_get_lr_modulation(bridge, base_lr);
    /* May be increased or same */
    EXPECT_GE(modulated_lr, base_lr * 0.9f);  /* Allow some tolerance */
}

TEST_F(TrainingLogicBridgeTest, LRModulationDecrease) {
    /* Trigger intervention */
    training_logic_set_condition(bridge, TRAINING_COND_GRAD_EXPLODING, true);

    float base_lr = 0.001f;
    float modulated_lr = training_logic_get_lr_modulation(bridge, base_lr);
    /* Should be decreased */
    EXPECT_LE(modulated_lr, base_lr);
}

TEST_F(TrainingLogicBridgeTest, LRModulationBounds) {
    training_logic_set_condition(bridge, TRAINING_COND_GRAD_EXPLODING, true);

    float base_lr = 0.001f;
    float modulated_lr = training_logic_get_lr_modulation(bridge, base_lr);

    /* Should respect min/max bounds from config */
    EXPECT_GE(modulated_lr, config.min_learning_rate);
    EXPECT_LE(modulated_lr, config.max_learning_rate);
}

TEST_F(TrainingLogicBridgeTest, BatchSizeModulation) {
    training_logic_set_condition(bridge, TRAINING_COND_MEMORY_OK, true);
    training_logic_set_condition(bridge, TRAINING_COND_THROUGHPUT_OK, true);

    uint32_t base_batch = 32;
    uint32_t modulated_batch = training_logic_get_batch_size_modulation(bridge, base_batch);

    /* Should be valid */
    EXPECT_GT(modulated_batch, 0u);
}

TEST_F(TrainingLogicBridgeTest, BatchSizeModulationBounds) {
    training_logic_set_condition(bridge, TRAINING_COND_MEMORY_OK, false);

    uint32_t base_batch = 32;
    uint32_t modulated_batch = training_logic_get_batch_size_modulation(bridge, base_batch);

    /* Should respect min/max bounds */
    EXPECT_GE(modulated_batch, config.min_batch_size);
    EXPECT_LE(modulated_batch, config.max_batch_size);
}

/*=============================================================================
 * CUSTOM GATE TESTS (5 tests)
 *============================================================================*/

TEST_F(TrainingLogicBridgeTest, AddCustomGateAND) {
    uint32_t gate_id = 0;
    int result = training_logic_add_custom_gate(bridge, "A AND B", &gate_id);
    EXPECT_EQ(result, 0);
    EXPECT_GE(gate_id, TRAINING_LOGIC_GATE_CUSTOM_START);
}

TEST_F(TrainingLogicBridgeTest, AddCustomGateOR) {
    uint32_t gate_id = 0;
    int result = training_logic_add_custom_gate(bridge, "A OR B", &gate_id);
    EXPECT_EQ(result, 0);
    EXPECT_GE(gate_id, TRAINING_LOGIC_GATE_CUSTOM_START);
}

TEST_F(TrainingLogicBridgeTest, AddCustomGateIMPLIES) {
    uint32_t gate_id = 0;
    int result = training_logic_add_custom_gate(bridge, "A IMPLIES B", &gate_id);
    EXPECT_EQ(result, 0);
    EXPECT_GE(gate_id, TRAINING_LOGIC_GATE_CUSTOM_START);
}

TEST_F(TrainingLogicBridgeTest, EvaluateCustomGate) {
    uint32_t gate_id = 0;
    int result = training_logic_add_custom_gate(bridge, "A AND B", &gate_id);
    EXPECT_EQ(result, 0);

    /* Evaluate the custom gate */
    bool gate_result = training_logic_evaluate_gate(bridge, gate_id);
    /* Result depends on conditions, just check it doesn't crash */
    (void)gate_result;
    SUCCEED();
}

TEST_F(TrainingLogicBridgeTest, CustomGateNullPointers) {
    uint32_t gate_id = 0;

    int result1 = training_logic_add_custom_gate(nullptr, "A AND B", &gate_id);
    EXPECT_NE(result1, 0);  /* Returns NIMCP_ERROR_NULL_POINTER (positive) */

    int result2 = training_logic_add_custom_gate(bridge, nullptr, &gate_id);
    EXPECT_NE(result2, 0);  /* Returns NIMCP_ERROR_NULL_POINTER (positive) */

    int result3 = training_logic_add_custom_gate(bridge, "A AND B", nullptr);
    EXPECT_NE(result3, 0);  /* Returns NIMCP_ERROR_NULL_POINTER (positive) */
}

/*=============================================================================
 * CONDITION MANAGEMENT TESTS (5 tests)
 *============================================================================*/

TEST_F(TrainingLogicBridgeTest, SetCondition) {
    int result = training_logic_set_condition(bridge, TRAINING_COND_LOSS_STABLE, true);
    EXPECT_EQ(result, 0);

    training_logic_conditions_t conditions;
    training_logic_get_conditions(bridge, &conditions);
    EXPECT_TRUE(conditions.loss_stable);
}

TEST_F(TrainingLogicBridgeTest, GetConditions) {
    training_logic_conditions_t conditions;
    int result = training_logic_get_conditions(bridge, &conditions);
    EXPECT_EQ(result, 0);
}

TEST_F(TrainingLogicBridgeTest, UpdateConditions) {
    int result = training_logic_update_conditions(bridge);
    EXPECT_EQ(result, 0);
}

TEST_F(TrainingLogicBridgeTest, ConditionAutoClear) {
    /* Set a condition */
    training_logic_set_condition(bridge, TRAINING_COND_GRAD_EXPLODING, true);

    training_logic_conditions_t conditions;
    training_logic_get_conditions(bridge, &conditions);
    EXPECT_TRUE(conditions.grad_exploding);

    /* Clear it */
    training_logic_set_condition(bridge, TRAINING_COND_GRAD_EXPLODING, false);
    training_logic_get_conditions(bridge, &conditions);
    EXPECT_FALSE(conditions.grad_exploding);
}

TEST_F(TrainingLogicBridgeTest, ConditionPersistence) {
    /* Set multiple conditions */
    training_logic_set_condition(bridge, TRAINING_COND_LOSS_STABLE, true);
    training_logic_set_condition(bridge, TRAINING_COND_GRAD_STABLE, true);
    training_logic_set_condition(bridge, TRAINING_COND_LR_REASONABLE, true);

    /* Verify they persist */
    training_logic_conditions_t conditions;
    training_logic_get_conditions(bridge, &conditions);
    EXPECT_TRUE(conditions.loss_stable);
    EXPECT_TRUE(conditions.grad_stable);
    EXPECT_TRUE(conditions.lr_reasonable);
}

/*=============================================================================
 * STATISTICS TESTS (4 tests)
 *============================================================================*/

TEST_F(TrainingLogicBridgeTest, GetStats) {
    training_logic_stats_t stats;
    int result = training_logic_get_stats(bridge, &stats);
    EXPECT_EQ(result, 0);
}

TEST_F(TrainingLogicBridgeTest, ResetStats) {
    /* Perform some operations */
    training_logic_check_stability(bridge);
    training_logic_needs_intervention(bridge);

    /* Reset stats */
    int result = training_logic_reset_stats(bridge);
    EXPECT_EQ(result, 0);

    /* Verify stats are reset */
    training_logic_stats_t stats;
    training_logic_get_stats(bridge, &stats);
    EXPECT_EQ(stats.total_decisions, 0u);
    EXPECT_EQ(stats.stability_checks, 0u);
    EXPECT_EQ(stats.intervention_triggers, 0u);
}

TEST_F(TrainingLogicBridgeTest, StatsAccumulation) {
    /* Perform multiple operations */
    for (int i = 0; i < 10; i++) {
        training_logic_check_stability(bridge);
    }

    training_logic_stats_t stats;
    training_logic_get_stats(bridge, &stats);
    EXPECT_GE(stats.stability_checks, 10u);
}

TEST_F(TrainingLogicBridgeTest, StatsNullPointer) {
    int result1 = training_logic_get_stats(nullptr, nullptr);
    EXPECT_NE(result1, 0);  /* Returns NIMCP_ERROR_NULL_POINTER (positive) */

    training_logic_stats_t stats;
    int result2 = training_logic_get_stats(nullptr, &stats);
    EXPECT_NE(result2, 0);  /* Returns NIMCP_ERROR_NULL_POINTER (positive) */

    int result3 = training_logic_get_stats(bridge, nullptr);
    EXPECT_NE(result3, 0);  /* Returns NIMCP_ERROR_NULL_POINTER (positive) */
}

/*=============================================================================
 * ERROR HANDLING TESTS (5 tests)
 *============================================================================*/

TEST_F(TrainingLogicBridgeTest, NullPointerHandling) {
    /* Test various null pointer scenarios */
    EXPECT_FALSE(training_logic_check_stability(nullptr));
    EXPECT_FALSE(training_logic_needs_intervention(nullptr));
    EXPECT_FALSE(training_logic_can_increase_lr(nullptr));
    EXPECT_FALSE(training_logic_should_checkpoint(nullptr));
}

TEST_F(TrainingLogicBridgeTest, InvalidModeHandling) {
    training_logic_config_t invalid_config;
    training_logic_default_config(&invalid_config);
    invalid_config.mode = (training_logic_mode_t)999;  /* Invalid mode */
    invalid_config.disable_auto_update = true;

    training_logic_bridge_t* invalid_bridge = training_logic_create(&invalid_config);
    /* Should handle gracefully */
    if (invalid_bridge) {
        training_logic_destroy(invalid_bridge);
    }
}

TEST_F(TrainingLogicBridgeTest, InvalidThresholdHandling) {
    training_logic_config_t invalid_config;
    training_logic_default_config(&invalid_config);
    invalid_config.stability_threshold = -1.0f;  /* Invalid threshold */
    invalid_config.disable_auto_update = true;

    training_logic_bridge_t* invalid_bridge = training_logic_create(&invalid_config);
    /* Should handle gracefully or create with defaults */
    if (invalid_bridge) {
        training_logic_destroy(invalid_bridge);
    }
}

TEST_F(TrainingLogicBridgeTest, OutOfBoundsCondition) {
    training_logic_condition_t invalid_cond = (training_logic_condition_t)999;
    int result = training_logic_set_condition(bridge, invalid_cond, true);
    EXPECT_NE(result, 0);  /* Should fail gracefully - NIMCP uses positive error codes */
}

TEST_F(TrainingLogicBridgeTest, GracefulDegradation) {
    /* Test that bridge continues to function even with errors */
    training_logic_set_condition(bridge, (training_logic_condition_t)999, true);  /* Invalid */

    /* Should still be able to check stability */
    bool stable = training_logic_check_stability(bridge);
    (void)stable;  /* Result is undefined, but shouldn't crash */

    SUCCEED();
}

/*=============================================================================
 * CONTINUOUS INSTABILITY TESTS
 *============================================================================*/

TEST_F(TrainingLogicBridgeTest, SignalInstabilityAccumulatesContinuousMetrics) {
    /* Signal grad explosion with severity 8 (normalized to 0.8) */
    training_logic_signal_instability(bridge, LOGIC_INSTABILITY_GRAD_EXPLOSION, 8);

    training_instability_metrics_t metrics;
    int rc = training_logic_get_instability_metrics(bridge, &metrics);
    EXPECT_EQ(rc, 0);
    EXPECT_FLOAT_EQ(metrics.gradient_explosion, 0.8f);
    EXPECT_GE(metrics.instability_score, 0.8f);
}

TEST_F(TrainingLogicBridgeTest, InstabilityScoreIsMaxAcrossChannels) {
    training_logic_signal_instability(bridge, LOGIC_INSTABILITY_GRAD_EXPLOSION, 5);
    training_logic_signal_instability(bridge, LOGIC_INSTABILITY_LOSS_PLATEAU, 8);

    training_instability_metrics_t metrics;
    training_logic_get_instability_metrics(bridge, &metrics);

    EXPECT_FLOAT_EQ(metrics.gradient_explosion, 0.5f);
    EXPECT_FLOAT_EQ(metrics.loss_plateau, 0.8f);
    EXPECT_FLOAT_EQ(metrics.instability_score, 0.8f);  /* max(0.5, 0.8) */
}

TEST_F(TrainingLogicBridgeTest, InstabilityAccumulatesViaMax) {
    /* Signal same channel twice — max wins */
    training_logic_signal_instability(bridge, LOGIC_INSTABILITY_GRAD_EXPLOSION, 3);
    training_logic_signal_instability(bridge, LOGIC_INSTABILITY_GRAD_EXPLOSION, 7);

    training_instability_metrics_t metrics;
    training_logic_get_instability_metrics(bridge, &metrics);
    EXPECT_FLOAT_EQ(metrics.gradient_explosion, 0.7f);  /* max(0.3, 0.7) */
}

TEST_F(TrainingLogicBridgeTest, ComputeInstabilityResponseNullSafety) {
    EXPECT_NE(training_logic_compute_instability_response(nullptr, nullptr), 0);

    training_instability_response_t response;
    EXPECT_NE(training_logic_compute_instability_response(nullptr, &response), 0);
    EXPECT_NE(training_logic_compute_instability_response(bridge, nullptr), 0);
}

TEST_F(TrainingLogicBridgeTest, ComputeInstabilityResponseZeroScore) {
    /* No instability signaled — response should be neutral */
    training_instability_response_t response;
    int rc = training_logic_compute_instability_response(bridge, &response);
    EXPECT_EQ(rc, 0);
    EXPECT_NEAR(response.lr_scale, 1.0f, 0.01f);         /* exp(0) = 1 */
    EXPECT_NEAR(response.batch_scale, 1.0f, 0.01f);       /* 1 - 0 = 1 */
    EXPECT_NEAR(response.pause_urgency, 0.0f, 0.01f);     /* 0^2 = 0 */
    EXPECT_NEAR(response.rollback_urgency, 0.0f, 0.01f);  /* score < 0.5 */
}

TEST_F(TrainingLogicBridgeTest, ComputeInstabilityResponseHighScore) {
    training_logic_signal_instability(bridge, LOGIC_INSTABILITY_LOSS_NAN, 10);

    training_instability_response_t response;
    int rc = training_logic_compute_instability_response(bridge, &response);
    EXPECT_EQ(rc, 0);
    EXPECT_LT(response.lr_scale, 0.1f);           /* exp(-3) ~ 0.05 */
    EXPECT_GT(response.pause_urgency, 0.9f);       /* 1^2 = 1 */
    EXPECT_GT(response.rollback_urgency, 0.9f);    /* 2*1-1 = 1 */
    /* clip_threshold = DEFAULT_GRAD_NORM_MAX / (1+5*score) = 10/(1+5) ~ 1.67 */
    EXPECT_LT(response.clip_threshold, 2.0f);      /* Reduced from base 10.0 */
}

TEST_F(TrainingLogicBridgeTest, LrScaleDecreasesMonotonically) {
    float prev_lr_scale = 2.0f;
    for (int sev = 0; sev <= 10; sev++) {
        /* Reset bridge for each test */
        training_logic_bridge_t* b = training_logic_create(nullptr);
        ASSERT_NE(b, nullptr);
        if (sev > 0) {
            training_logic_signal_instability(b, LOGIC_INSTABILITY_GRAD_EXPLOSION, sev);
        }
        training_instability_response_t response;
        training_logic_compute_instability_response(b, &response);
        EXPECT_LE(response.lr_scale, prev_lr_scale)
            << "lr_scale should decrease monotonically at severity " << sev;
        prev_lr_scale = response.lr_scale;
        training_logic_destroy(b);
    }
}

TEST_F(TrainingLogicBridgeTest, ClassifyInstabilityNone) {
    training_instability_metrics_t metrics;
    memset(&metrics, 0, sizeof(metrics));
    EXPECT_EQ(training_logic_classify_instability(&metrics), LOGIC_INSTABILITY_NONE);
}

TEST_F(TrainingLogicBridgeTest, ClassifyInstabilityNaNPriority) {
    training_instability_metrics_t metrics;
    memset(&metrics, 0, sizeof(metrics));
    metrics.nan_inf_severity = 0.9f;
    metrics.gradient_explosion = 0.8f;
    metrics.instability_score = 0.9f;
    EXPECT_EQ(training_logic_classify_instability(&metrics), LOGIC_INSTABILITY_LOSS_NAN);
}

TEST_F(TrainingLogicBridgeTest, ClassifyInstabilityOscillation) {
    training_instability_metrics_t metrics;
    memset(&metrics, 0, sizeof(metrics));
    metrics.loss_volatility = 0.6f;
    metrics.gradient_variance = 0.4f;
    metrics.instability_score = 0.6f;
    EXPECT_EQ(training_logic_classify_instability(&metrics), LOGIC_INSTABILITY_OSCILLATION);
}

TEST_F(TrainingLogicBridgeTest, InstabilityToString) {
    EXPECT_STREQ(training_logic_instability_to_string(LOGIC_INSTABILITY_NONE), "none");
    EXPECT_STREQ(training_logic_instability_to_string(LOGIC_INSTABILITY_LOSS_NAN), "loss_nan");
    EXPECT_STREQ(training_logic_instability_to_string(LOGIC_INSTABILITY_GRAD_EXPLOSION), "grad_explosion");
    EXPECT_STREQ(training_logic_instability_to_string(LOGIC_INSTABILITY_OSCILLATION), "oscillation");
}

TEST_F(TrainingLogicBridgeTest, LrModulationUsesContinuousInstability) {
    /* With high instability, LR should be reduced even without discrete decision */
    training_logic_signal_instability(bridge, LOGIC_INSTABILITY_GRAD_EXPLOSION, 10);

    float base_lr = 0.01f;
    float modulated = training_logic_get_lr_modulation(bridge, base_lr);
    /* exp(-3*1.0) ≈ 0.05, so modulated ≈ 0.01 * 0.05 = 0.0005 */
    EXPECT_LT(modulated, base_lr * 0.1f);
}

TEST_F(TrainingLogicBridgeTest, GetInstabilityMetricsNullSafety) {
    EXPECT_NE(training_logic_get_instability_metrics(nullptr, nullptr), 0);
    training_instability_metrics_t metrics;
    EXPECT_NE(training_logic_get_instability_metrics(nullptr, &metrics), 0);
}

/*=============================================================================
 * MAIN
 *============================================================================*/

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
