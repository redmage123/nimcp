/**
 * @file test_cortical_logic_integration.cpp
 * @brief Cross-bridge integration tests: Cortical-Training → Training-Logic
 *
 * WHAT: Tests bidirectional integration between cortical and logic bridges
 * WHY:  Verify cortical state correctly affects logic gate decisions
 * HOW:  Create both bridges, connect them, verify condition propagation
 *
 * TEST COVERAGE:
 * - Connection lifecycle (4 tests)
 * - Condition propagation (6 tests)
 * - Decision impact (5 tests)
 *
 * TOTAL: 15 tests
 *
 * @author NIMCP Development Team
 * @date 2025-12-20
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

// Headers have their own extern "C" guards
#include "middleware/training/nimcp_cortical_training_bridge.h"
#include "middleware/training/nimcp_training_logic_bridge.h"
#include "utils/error/nimcp_error_codes.h"

class CorticalLogicIntegrationTest : public ::testing::Test {
protected:
    cortical_training_bridge_t* cortical_bridge;
    training_logic_bridge_t* logic_bridge;
    cortical_training_config_t cortical_config;
    training_logic_config_t logic_config;

    void SetUp() override {
        cortical_training_default_config(&cortical_config);
        cortical_config.enable_bio_async = false;
        cortical_bridge = cortical_training_create(&cortical_config);
        ASSERT_NE(cortical_bridge, nullptr);

        training_logic_default_config(&logic_config);
        logic_config.enable_bio_async = false;
        logic_config.disable_auto_update = true;  /* Manual condition control */
        logic_bridge = training_logic_create(&logic_config);
        ASSERT_NE(logic_bridge, nullptr);
    }

    void TearDown() override {
        if (logic_bridge) {
            training_logic_destroy(logic_bridge);
            logic_bridge = nullptr;
        }
        if (cortical_bridge) {
            cortical_training_destroy(cortical_bridge);
            cortical_bridge = nullptr;
        }
    }
};

//=============================================================================
// Connection Lifecycle (4 tests)
//=============================================================================

TEST_F(CorticalLogicIntegrationTest, ConnectCorticalToLogic) {
    EXPECT_EQ(training_logic_start(logic_bridge), 0);
    EXPECT_EQ(cortical_training_start(cortical_bridge), 0);

    EXPECT_EQ(training_logic_connect_cortical_training(logic_bridge, cortical_bridge), 0);
    SUCCEED();
}

TEST_F(CorticalLogicIntegrationTest, DisconnectCorticalFromLogic) {
    EXPECT_EQ(training_logic_start(logic_bridge), 0);
    EXPECT_EQ(cortical_training_start(cortical_bridge), 0);

    EXPECT_EQ(training_logic_connect_cortical_training(logic_bridge, cortical_bridge), 0);
    EXPECT_EQ(training_logic_connect_cortical_training(logic_bridge, nullptr), 0);
    SUCCEED();
}

TEST_F(CorticalLogicIntegrationTest, ConnectNullLogicReturnsError) {
    EXPECT_NE(training_logic_connect_cortical_training(nullptr, cortical_bridge), 0);
}

TEST_F(CorticalLogicIntegrationTest, ReconnectCorticalBridge) {
    EXPECT_EQ(training_logic_start(logic_bridge), 0);
    EXPECT_EQ(cortical_training_start(cortical_bridge), 0);

    EXPECT_EQ(training_logic_connect_cortical_training(logic_bridge, cortical_bridge), 0);
    EXPECT_EQ(training_logic_connect_cortical_training(logic_bridge, nullptr), 0);
    EXPECT_EQ(training_logic_connect_cortical_training(logic_bridge, cortical_bridge), 0);
    SUCCEED();
}

//=============================================================================
// Condition Propagation (6 tests)
//=============================================================================

TEST_F(CorticalLogicIntegrationTest, StablePredictionsSetsCorticalStable) {
    EXPECT_EQ(training_logic_start(logic_bridge), 0);
    EXPECT_EQ(cortical_training_start(cortical_bridge), 0);
    EXPECT_EQ(training_logic_connect_cortical_training(logic_bridge, cortical_bridge), 0);

    /* Set stable predictions */
    cortical_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.predictions_stable = true;
    effects.valid = true;
    EXPECT_EQ(cortical_training_set_effects_for_testing(cortical_bridge, &effects), 0);

    /* Trigger condition update */
    EXPECT_EQ(training_logic_update_conditions(logic_bridge), 0);

    training_logic_conditions_t cond;
    EXPECT_EQ(training_logic_get_conditions(logic_bridge, &cond), 0);
    EXPECT_TRUE(cond.cortical_stable);
}

TEST_F(CorticalLogicIntegrationTest, UnstablePredictionsClearsCorticalStable) {
    EXPECT_EQ(training_logic_start(logic_bridge), 0);
    EXPECT_EQ(cortical_training_start(cortical_bridge), 0);
    EXPECT_EQ(training_logic_connect_cortical_training(logic_bridge, cortical_bridge), 0);

    /* Set unstable predictions */
    cortical_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.predictions_stable = false;
    effects.valid = true;
    EXPECT_EQ(cortical_training_set_effects_for_testing(cortical_bridge, &effects), 0);

    /* Trigger condition update */
    EXPECT_EQ(training_logic_update_conditions(logic_bridge), 0);

    training_logic_conditions_t cond;
    EXPECT_EQ(training_logic_get_conditions(logic_bridge, &cond), 0);
    EXPECT_FALSE(cond.cortical_stable);
}

TEST_F(CorticalLogicIntegrationTest, HighBurstRateSetsPredictionsOK) {
    EXPECT_EQ(training_logic_start(logic_bridge), 0);
    EXPECT_EQ(cortical_training_start(cortical_bridge), 0);
    EXPECT_EQ(training_logic_connect_cortical_training(logic_bridge, cortical_bridge), 0);

    /* Set high burst rate (> 0.5) */
    cortical_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.burst_rate = 0.8f;
    effects.valid = true;
    EXPECT_EQ(cortical_training_set_effects_for_testing(cortical_bridge, &effects), 0);

    /* Trigger condition update */
    EXPECT_EQ(training_logic_update_conditions(logic_bridge), 0);

    training_logic_conditions_t cond;
    EXPECT_EQ(training_logic_get_conditions(logic_bridge, &cond), 0);
    EXPECT_TRUE(cond.predictions_ok);
}

TEST_F(CorticalLogicIntegrationTest, LowBurstRateClearsPredictionsOK) {
    EXPECT_EQ(training_logic_start(logic_bridge), 0);
    EXPECT_EQ(cortical_training_start(cortical_bridge), 0);
    EXPECT_EQ(training_logic_connect_cortical_training(logic_bridge, cortical_bridge), 0);

    /* Set low burst rate (< 0.5) */
    cortical_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.burst_rate = 0.3f;
    effects.valid = true;
    EXPECT_EQ(cortical_training_set_effects_for_testing(cortical_bridge, &effects), 0);

    /* Trigger condition update */
    EXPECT_EQ(training_logic_update_conditions(logic_bridge), 0);

    training_logic_conditions_t cond;
    EXPECT_EQ(training_logic_get_conditions(logic_bridge, &cond), 0);
    EXPECT_FALSE(cond.predictions_ok);
}

TEST_F(CorticalLogicIntegrationTest, ValidEffectsUpdateConditions) {
    EXPECT_EQ(training_logic_start(logic_bridge), 0);
    EXPECT_EQ(cortical_training_start(cortical_bridge), 0);
    EXPECT_EQ(training_logic_connect_cortical_training(logic_bridge, cortical_bridge), 0);

    /* First, explicitly set conditions to false */
    EXPECT_EQ(training_logic_set_condition(logic_bridge, TRAINING_COND_CORTICAL_STABLE, false), 0);
    EXPECT_EQ(training_logic_set_condition(logic_bridge, TRAINING_COND_PREDICTIONS_OK, false), 0);

    training_logic_conditions_t cond_before;
    EXPECT_EQ(training_logic_get_conditions(logic_bridge, &cond_before), 0);
    EXPECT_FALSE(cond_before.cortical_stable);
    EXPECT_FALSE(cond_before.predictions_ok);

    /* Set stable predictions with valid effects */
    cortical_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.predictions_stable = true;
    effects.burst_rate = 0.9f;
    effects.valid = true;  /* Valid */
    EXPECT_EQ(cortical_training_set_effects_for_testing(cortical_bridge, &effects), 0);

    /* Trigger condition update */
    EXPECT_EQ(training_logic_update_conditions(logic_bridge), 0);

    training_logic_conditions_t cond_after;
    EXPECT_EQ(training_logic_get_conditions(logic_bridge, &cond_after), 0);
    /* Conditions SHOULD be updated from valid effects */
    EXPECT_TRUE(cond_after.cortical_stable);
    EXPECT_TRUE(cond_after.predictions_ok);
}

TEST_F(CorticalLogicIntegrationTest, ConditionUpdatesDynamically) {
    EXPECT_EQ(training_logic_start(logic_bridge), 0);
    EXPECT_EQ(cortical_training_start(cortical_bridge), 0);
    EXPECT_EQ(training_logic_connect_cortical_training(logic_bridge, cortical_bridge), 0);

    /* First: unstable, low burst */
    cortical_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.predictions_stable = false;
    effects.burst_rate = 0.2f;
    effects.valid = true;
    EXPECT_EQ(cortical_training_set_effects_for_testing(cortical_bridge, &effects), 0);
    EXPECT_EQ(training_logic_update_conditions(logic_bridge), 0);

    training_logic_conditions_t cond;
    EXPECT_EQ(training_logic_get_conditions(logic_bridge, &cond), 0);
    EXPECT_FALSE(cond.cortical_stable);
    EXPECT_FALSE(cond.predictions_ok);

    /* Second: stable, high burst */
    effects.predictions_stable = true;
    effects.burst_rate = 0.8f;
    EXPECT_EQ(cortical_training_set_effects_for_testing(cortical_bridge, &effects), 0);
    EXPECT_EQ(training_logic_update_conditions(logic_bridge), 0);

    EXPECT_EQ(training_logic_get_conditions(logic_bridge, &cond), 0);
    EXPECT_TRUE(cond.cortical_stable);
    EXPECT_TRUE(cond.predictions_ok);
}

//=============================================================================
// Decision Impact (5 tests)
//=============================================================================

TEST_F(CorticalLogicIntegrationTest, CorticalStableAffectsDecision) {
    EXPECT_EQ(training_logic_start(logic_bridge), 0);
    EXPECT_EQ(cortical_training_start(cortical_bridge), 0);
    EXPECT_EQ(training_logic_connect_cortical_training(logic_bridge, cortical_bridge), 0);

    /* Set up stable conditions */
    EXPECT_EQ(training_logic_set_condition(logic_bridge, TRAINING_COND_LOSS_STABLE, true), 0);
    EXPECT_EQ(training_logic_set_condition(logic_bridge, TRAINING_COND_GRAD_STABLE, true), 0);
    EXPECT_EQ(training_logic_set_condition(logic_bridge, TRAINING_COND_LR_REASONABLE, true), 0);
    EXPECT_EQ(training_logic_set_condition(logic_bridge, TRAINING_COND_CORTICAL_STABLE, true), 0);
    EXPECT_EQ(training_logic_set_condition(logic_bridge, TRAINING_COND_PREDICTIONS_OK, true), 0);

    /* Stability check should pass */
    EXPECT_TRUE(training_logic_check_stability(logic_bridge));
}

TEST_F(CorticalLogicIntegrationTest, CorticalConditionsInDecision) {
    EXPECT_EQ(training_logic_start(logic_bridge), 0);
    EXPECT_EQ(cortical_training_start(cortical_bridge), 0);
    EXPECT_EQ(training_logic_connect_cortical_training(logic_bridge, cortical_bridge), 0);

    /* Set all stable conditions */
    EXPECT_EQ(training_logic_set_condition(logic_bridge, TRAINING_COND_LOSS_STABLE, true), 0);
    EXPECT_EQ(training_logic_set_condition(logic_bridge, TRAINING_COND_GRAD_STABLE, true), 0);
    EXPECT_EQ(training_logic_set_condition(logic_bridge, TRAINING_COND_LR_REASONABLE, true), 0);
    EXPECT_EQ(training_logic_set_condition(logic_bridge, TRAINING_COND_MEMORY_OK, true), 0);
    EXPECT_EQ(training_logic_set_condition(logic_bridge, TRAINING_COND_THROUGHPUT_OK, true), 0);
    EXPECT_EQ(training_logic_set_condition(logic_bridge, TRAINING_COND_CORTICAL_STABLE, true), 0);
    EXPECT_EQ(training_logic_set_condition(logic_bridge, TRAINING_COND_PREDICTIONS_OK, true), 0);

    training_logic_decision_t decision;
    EXPECT_EQ(training_logic_get_decision(logic_bridge, &decision), 0);
    /* Should be CONTINUE or similar stable decision */
    EXPECT_TRUE(decision.stability_check_passed);
}

TEST_F(CorticalLogicIntegrationTest, LRModulationWithCorticalIntegration) {
    EXPECT_EQ(training_logic_start(logic_bridge), 0);
    EXPECT_EQ(cortical_training_start(cortical_bridge), 0);
    EXPECT_EQ(training_logic_connect_cortical_training(logic_bridge, cortical_bridge), 0);

    /* Set cortical state */
    cortical_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.predictions_stable = true;
    effects.burst_rate = 0.8f;
    effects.valid = true;
    EXPECT_EQ(cortical_training_set_effects_for_testing(cortical_bridge, &effects), 0);

    /* Get modulated LR - should return reasonable value */
    float lr = training_logic_get_lr_modulation(logic_bridge, 0.001f);
    EXPECT_GT(lr, 0.0f);
    EXPECT_LT(lr, 0.01f);
}

TEST_F(CorticalLogicIntegrationTest, TrainingLoopWithCorticalLogicIntegration) {
    EXPECT_EQ(training_logic_start(logic_bridge), 0);
    EXPECT_EQ(cortical_training_start(cortical_bridge), 0);
    EXPECT_EQ(training_logic_connect_cortical_training(logic_bridge, cortical_bridge), 0);

    /* Simulate training loop with improving cortical state */
    for (int step = 0; step < 50; ++step) {
        /* Cortical state improves over time */
        cortical_training_effects_t effects;
        memset(&effects, 0, sizeof(effects));
        effects.predictions_stable = (step > 25);
        effects.burst_rate = 0.2f + 0.6f * (step / 50.0f);
        effects.valid = true;
        EXPECT_EQ(cortical_training_set_effects_for_testing(cortical_bridge, &effects), 0);

        /* Update logic conditions */
        EXPECT_EQ(training_logic_update_conditions(logic_bridge), 0);

        /* Set basic stability conditions */
        EXPECT_EQ(training_logic_set_condition(logic_bridge, TRAINING_COND_LOSS_STABLE, true), 0);
        EXPECT_EQ(training_logic_set_condition(logic_bridge, TRAINING_COND_GRAD_STABLE, true), 0);
        EXPECT_EQ(training_logic_set_condition(logic_bridge, TRAINING_COND_LR_REASONABLE, true), 0);

        /* Get decision */
        training_logic_decision_t decision;
        EXPECT_EQ(training_logic_get_decision(logic_bridge, &decision), 0);
    }
}

TEST_F(CorticalLogicIntegrationTest, ConditionToStringForCortical) {
    const char* stable = training_logic_condition_to_string(TRAINING_COND_CORTICAL_STABLE);
    EXPECT_STREQ(stable, "cortical_stable");

    const char* pred = training_logic_condition_to_string(TRAINING_COND_PREDICTIONS_OK);
    EXPECT_STREQ(pred, "predictions_ok");
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
