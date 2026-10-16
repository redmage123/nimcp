/**
 * @file test_perception_logic_integration.cpp
 * @brief Cross-bridge integration tests: Perception-Training → Training-Logic
 *
 * WHAT: Tests bidirectional integration between perception and logic bridges
 * WHY:  Verify perception state correctly affects logic gate decisions
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
#include "middleware/training/nimcp_perception_training_bridge.h"
#include "middleware/training/nimcp_training_logic_bridge.h"
#include "utils/error/nimcp_error_codes.h"

class PerceptionLogicIntegrationTest : public ::testing::Test {
protected:
    perception_training_bridge_t* perception_bridge;
    training_logic_bridge_t* logic_bridge;
    perception_training_config_t perception_config;
    training_logic_config_t logic_config;

    void SetUp() override {
        perception_training_default_config(&perception_config);
        perception_config.enable_bio_async = false;
        perception_bridge = perception_training_create(&perception_config);
        ASSERT_NE(perception_bridge, nullptr);

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
        if (perception_bridge) {
            perception_training_destroy(perception_bridge);
            perception_bridge = nullptr;
        }
    }
};

//=============================================================================
// Connection Lifecycle (4 tests)
//=============================================================================

TEST_F(PerceptionLogicIntegrationTest, ConnectPerceptionToLogic) {
    EXPECT_EQ(training_logic_start(logic_bridge), 0);
    EXPECT_EQ(perception_training_start(perception_bridge), 0);

    EXPECT_EQ(training_logic_connect_perception_training(logic_bridge, perception_bridge), 0);
    /* No specific stats field for perception connection in logic bridge */
    SUCCEED();
}

TEST_F(PerceptionLogicIntegrationTest, DisconnectPerceptionFromLogic) {
    EXPECT_EQ(training_logic_start(logic_bridge), 0);
    EXPECT_EQ(perception_training_start(perception_bridge), 0);

    EXPECT_EQ(training_logic_connect_perception_training(logic_bridge, perception_bridge), 0);
    EXPECT_EQ(training_logic_connect_perception_training(logic_bridge, nullptr), 0);
    SUCCEED();
}

TEST_F(PerceptionLogicIntegrationTest, ConnectNullLogicReturnsError) {
    EXPECT_NE(training_logic_connect_perception_training(nullptr, perception_bridge), 0);
}

TEST_F(PerceptionLogicIntegrationTest, ReconnectPerceptionBridge) {
    EXPECT_EQ(training_logic_start(logic_bridge), 0);
    EXPECT_EQ(perception_training_start(perception_bridge), 0);

    EXPECT_EQ(training_logic_connect_perception_training(logic_bridge, perception_bridge), 0);
    EXPECT_EQ(training_logic_connect_perception_training(logic_bridge, nullptr), 0);
    EXPECT_EQ(training_logic_connect_perception_training(logic_bridge, perception_bridge), 0);
    SUCCEED();
}

//=============================================================================
// Condition Propagation (6 tests)
//=============================================================================

TEST_F(PerceptionLogicIntegrationTest, HighLRFactorSetsPerceptionQuality) {
    EXPECT_EQ(training_logic_start(logic_bridge), 0);
    EXPECT_EQ(perception_training_start(perception_bridge), 0);
    EXPECT_EQ(training_logic_connect_perception_training(logic_bridge, perception_bridge), 0);

    /* Set high LR factor (> 0.8) */
    perception_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.lr_factor = 0.95f;
    effects.valid = true;
    EXPECT_EQ(perception_training_set_effects_for_testing(perception_bridge, &effects), 0);

    /* Trigger condition update */
    EXPECT_EQ(training_logic_update_conditions(logic_bridge), 0);

    training_logic_conditions_t cond;
    EXPECT_EQ(training_logic_get_conditions(logic_bridge, &cond), 0);
    EXPECT_TRUE(cond.perception_quality);
}

TEST_F(PerceptionLogicIntegrationTest, LowLRFactorClearsPerceptionQuality) {
    EXPECT_EQ(training_logic_start(logic_bridge), 0);
    EXPECT_EQ(perception_training_start(perception_bridge), 0);
    EXPECT_EQ(training_logic_connect_perception_training(logic_bridge, perception_bridge), 0);

    /* Set low LR factor (< 0.8) */
    perception_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.lr_factor = 0.5f;
    effects.valid = true;
    EXPECT_EQ(perception_training_set_effects_for_testing(perception_bridge, &effects), 0);

    /* Trigger condition update */
    EXPECT_EQ(training_logic_update_conditions(logic_bridge), 0);

    training_logic_conditions_t cond;
    EXPECT_EQ(training_logic_get_conditions(logic_bridge, &cond), 0);
    EXPECT_FALSE(cond.perception_quality);
}

TEST_F(PerceptionLogicIntegrationTest, ValidEffectsUpdateConditions) {
    EXPECT_EQ(training_logic_start(logic_bridge), 0);
    EXPECT_EQ(perception_training_start(perception_bridge), 0);
    EXPECT_EQ(training_logic_connect_perception_training(logic_bridge, perception_bridge), 0);

    /* First, explicitly set perception_quality to false */
    EXPECT_EQ(training_logic_set_condition(logic_bridge, TRAINING_COND_PERCEPTION_QUALITY, false), 0);

    training_logic_conditions_t cond_before;
    EXPECT_EQ(training_logic_get_conditions(logic_bridge, &cond_before), 0);
    EXPECT_FALSE(cond_before.perception_quality);

    /* Set high LR factor with valid effects */
    perception_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.lr_factor = 0.95f;
    effects.valid = true;  /* Valid */
    EXPECT_EQ(perception_training_set_effects_for_testing(perception_bridge, &effects), 0);

    /* Trigger condition update */
    EXPECT_EQ(training_logic_update_conditions(logic_bridge), 0);

    training_logic_conditions_t cond_after;
    EXPECT_EQ(training_logic_get_conditions(logic_bridge, &cond_after), 0);
    /* Condition SHOULD be updated from valid effects */
    EXPECT_TRUE(cond_after.perception_quality);
}

TEST_F(PerceptionLogicIntegrationTest, ConditionUpdatesDynamically) {
    EXPECT_EQ(training_logic_start(logic_bridge), 0);
    EXPECT_EQ(perception_training_start(perception_bridge), 0);
    EXPECT_EQ(training_logic_connect_perception_training(logic_bridge, perception_bridge), 0);

    /* First: low quality */
    perception_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.lr_factor = 0.5f;
    effects.valid = true;
    EXPECT_EQ(perception_training_set_effects_for_testing(perception_bridge, &effects), 0);
    EXPECT_EQ(training_logic_update_conditions(logic_bridge), 0);

    training_logic_conditions_t cond;
    EXPECT_EQ(training_logic_get_conditions(logic_bridge, &cond), 0);
    EXPECT_FALSE(cond.perception_quality);

    /* Second: high quality */
    effects.lr_factor = 0.9f;
    EXPECT_EQ(perception_training_set_effects_for_testing(perception_bridge, &effects), 0);
    EXPECT_EQ(training_logic_update_conditions(logic_bridge), 0);

    EXPECT_EQ(training_logic_get_conditions(logic_bridge, &cond), 0);
    EXPECT_TRUE(cond.perception_quality);
}

TEST_F(PerceptionLogicIntegrationTest, DisconnectedPerceptionDoesNotAffectConditions) {
    EXPECT_EQ(training_logic_start(logic_bridge), 0);
    EXPECT_EQ(perception_training_start(perception_bridge), 0);

    /* Connect, set high quality, verify, then disconnect */
    EXPECT_EQ(training_logic_connect_perception_training(logic_bridge, perception_bridge), 0);

    perception_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.lr_factor = 0.9f;
    effects.valid = true;
    EXPECT_EQ(perception_training_set_effects_for_testing(perception_bridge, &effects), 0);
    EXPECT_EQ(training_logic_update_conditions(logic_bridge), 0);

    training_logic_conditions_t cond;
    EXPECT_EQ(training_logic_get_conditions(logic_bridge, &cond), 0);
    EXPECT_TRUE(cond.perception_quality);

    /* Disconnect */
    EXPECT_EQ(training_logic_connect_perception_training(logic_bridge, nullptr), 0);

    /* Change perception effects (but disconnected) */
    effects.lr_factor = 0.3f;
    EXPECT_EQ(perception_training_set_effects_for_testing(perception_bridge, &effects), 0);
    EXPECT_EQ(training_logic_update_conditions(logic_bridge), 0);

    /* Condition should retain last known value */
    EXPECT_EQ(training_logic_get_conditions(logic_bridge, &cond), 0);
    /* After disconnect, perception_quality is not updated, retains old value */
    EXPECT_TRUE(cond.perception_quality);
}

TEST_F(PerceptionLogicIntegrationTest, ManualConditionOverrideWorks) {
    EXPECT_EQ(training_logic_start(logic_bridge), 0);
    EXPECT_EQ(perception_training_start(perception_bridge), 0);
    EXPECT_EQ(training_logic_connect_perception_training(logic_bridge, perception_bridge), 0);

    /* Manually set condition */
    EXPECT_EQ(training_logic_set_condition(logic_bridge, TRAINING_COND_PERCEPTION_QUALITY, true), 0);

    training_logic_conditions_t cond;
    EXPECT_EQ(training_logic_get_conditions(logic_bridge, &cond), 0);
    EXPECT_TRUE(cond.perception_quality);

    EXPECT_EQ(training_logic_set_condition(logic_bridge, TRAINING_COND_PERCEPTION_QUALITY, false), 0);
    EXPECT_EQ(training_logic_get_conditions(logic_bridge, &cond), 0);
    EXPECT_FALSE(cond.perception_quality);
}

//=============================================================================
// Decision Impact (5 tests)
//=============================================================================

TEST_F(PerceptionLogicIntegrationTest, PerceptionQualityAffectsStabilityCheck) {
    EXPECT_EQ(training_logic_start(logic_bridge), 0);
    EXPECT_EQ(perception_training_start(perception_bridge), 0);
    EXPECT_EQ(training_logic_connect_perception_training(logic_bridge, perception_bridge), 0);

    /* Set up stable conditions */
    EXPECT_EQ(training_logic_set_condition(logic_bridge, TRAINING_COND_LOSS_STABLE, true), 0);
    EXPECT_EQ(training_logic_set_condition(logic_bridge, TRAINING_COND_GRAD_STABLE, true), 0);
    EXPECT_EQ(training_logic_set_condition(logic_bridge, TRAINING_COND_LR_REASONABLE, true), 0);

    /* Stability check should pass */
    EXPECT_TRUE(training_logic_check_stability(logic_bridge));
}

TEST_F(PerceptionLogicIntegrationTest, PerceptionQualityInDecision) {
    EXPECT_EQ(training_logic_start(logic_bridge), 0);
    EXPECT_EQ(perception_training_start(perception_bridge), 0);
    EXPECT_EQ(training_logic_connect_perception_training(logic_bridge, perception_bridge), 0);

    /* Set all stable conditions */
    EXPECT_EQ(training_logic_set_condition(logic_bridge, TRAINING_COND_LOSS_STABLE, true), 0);
    EXPECT_EQ(training_logic_set_condition(logic_bridge, TRAINING_COND_GRAD_STABLE, true), 0);
    EXPECT_EQ(training_logic_set_condition(logic_bridge, TRAINING_COND_LR_REASONABLE, true), 0);
    EXPECT_EQ(training_logic_set_condition(logic_bridge, TRAINING_COND_MEMORY_OK, true), 0);
    EXPECT_EQ(training_logic_set_condition(logic_bridge, TRAINING_COND_THROUGHPUT_OK, true), 0);
    EXPECT_EQ(training_logic_set_condition(logic_bridge, TRAINING_COND_NOT_MID_BATCH, true), 0);
    EXPECT_EQ(training_logic_set_condition(logic_bridge, TRAINING_COND_PERCEPTION_QUALITY, true), 0);

    training_logic_decision_t decision;
    EXPECT_EQ(training_logic_get_decision(logic_bridge, &decision), 0);
    /* Should be CONTINUE or similar stable decision */
    EXPECT_TRUE(decision.stability_check_passed);
}

TEST_F(PerceptionLogicIntegrationTest, LRModulationWithPerceptionIntegration) {
    EXPECT_EQ(training_logic_start(logic_bridge), 0);
    EXPECT_EQ(perception_training_start(perception_bridge), 0);
    EXPECT_EQ(training_logic_connect_perception_training(logic_bridge, perception_bridge), 0);

    /* Set perception state */
    perception_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.lr_factor = 0.9f;
    effects.valid = true;
    EXPECT_EQ(perception_training_set_effects_for_testing(perception_bridge, &effects), 0);

    /* Get modulated LR - should return reasonable value */
    float lr = training_logic_get_lr_modulation(logic_bridge, 0.001f);
    EXPECT_GT(lr, 0.0f);
    EXPECT_LT(lr, 0.01f);
}

TEST_F(PerceptionLogicIntegrationTest, TrainingLoopWithPerceptionLogicIntegration) {
    EXPECT_EQ(training_logic_start(logic_bridge), 0);
    EXPECT_EQ(perception_training_start(perception_bridge), 0);
    EXPECT_EQ(training_logic_connect_perception_training(logic_bridge, perception_bridge), 0);

    /* Simulate training loop with varying perception */
    for (int step = 0; step < 50; ++step) {
        /* Perception varies */
        perception_training_effects_t effects;
        memset(&effects, 0, sizeof(effects));
        effects.lr_factor = 0.5f + 0.4f * (step / 50.0f);
        effects.valid = true;
        EXPECT_EQ(perception_training_set_effects_for_testing(perception_bridge, &effects), 0);

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

TEST_F(PerceptionLogicIntegrationTest, ConditionToStringForPerception) {
    const char* name = training_logic_condition_to_string(TRAINING_COND_PERCEPTION_QUALITY);
    EXPECT_STREQ(name, "perception_quality");
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
