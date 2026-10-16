/**
 * @file test_executive_sleep_bridge.cpp
 * @brief Unit tests for Executive-Sleep Integration Bridge
 * @version 1.0.0
 * @date 2025-12-17
 */

#include <gtest/gtest.h>
#include "cognitive/executive/nimcp_executive_sleep_bridge.h"

/* ============================================================================
 * Helper Function Tests
 * ============================================================================ */

class ExecutiveSleepHelperTest : public ::testing::Test {};

/* Inhibition Factor Tests */
TEST_F(ExecutiveSleepHelperTest, InhibitionFactorAwake) {
    EXPECT_FLOAT_EQ(executive_sleep_inhibition_for_state(SLEEP_STATE_AWAKE),
                    EXEC_SLEEP_INHIBITION_AWAKE);
}

TEST_F(ExecutiveSleepHelperTest, InhibitionFactorDrowsy) {
    float inhib = executive_sleep_inhibition_for_state(SLEEP_STATE_DROWSY);
    EXPECT_FLOAT_EQ(inhib, EXEC_SLEEP_INHIBITION_DROWSY);
    EXPECT_LT(inhib, 1.0f) << "Inhibition impaired when drowsy";
}

TEST_F(ExecutiveSleepHelperTest, InhibitionFactorLightNREM) {
    float inhib = executive_sleep_inhibition_for_state(SLEEP_STATE_LIGHT_NREM);
    EXPECT_FLOAT_EQ(inhib, EXEC_SLEEP_INHIBITION_LIGHT_NREM);
    EXPECT_LT(inhib, 0.2f) << "Minimal inhibition in light NREM";
}

TEST_F(ExecutiveSleepHelperTest, InhibitionFactorDeepNREM) {
    float inhib = executive_sleep_inhibition_for_state(SLEEP_STATE_DEEP_NREM);
    EXPECT_FLOAT_EQ(inhib, EXEC_SLEEP_INHIBITION_DEEP_NREM);
    EXPECT_FLOAT_EQ(inhib, 0.0f) << "No inhibitory control in deep NREM";
}

TEST_F(ExecutiveSleepHelperTest, InhibitionFactorREM) {
    float inhib = executive_sleep_inhibition_for_state(SLEEP_STATE_REM);
    EXPECT_FLOAT_EQ(inhib, EXEC_SLEEP_INHIBITION_REM);
    EXPECT_GT(inhib, 0.0f) << "Some inhibition in REM (but reduced -> dream bizarreness)";
}

/* Flexibility Factor Tests */
TEST_F(ExecutiveSleepHelperTest, FlexibilityFactorAwake) {
    EXPECT_FLOAT_EQ(executive_sleep_flexibility_for_state(SLEEP_STATE_AWAKE),
                    EXEC_SLEEP_FLEXIBILITY_AWAKE);
}

TEST_F(ExecutiveSleepHelperTest, FlexibilityFactorDrowsy) {
    float flex = executive_sleep_flexibility_for_state(SLEEP_STATE_DROWSY);
    EXPECT_FLOAT_EQ(flex, EXEC_SLEEP_FLEXIBILITY_DROWSY);
    EXPECT_LT(flex, 1.0f) << "Flexibility impaired when drowsy";
}

TEST_F(ExecutiveSleepHelperTest, FlexibilityFactorNREM) {
    float flex = executive_sleep_flexibility_for_state(SLEEP_STATE_LIGHT_NREM);
    EXPECT_FLOAT_EQ(flex, EXEC_SLEEP_FLEXIBILITY_NREM);
    EXPECT_FLOAT_EQ(flex, 0.0f) << "No flexibility during NREM";
}

TEST_F(ExecutiveSleepHelperTest, FlexibilityFactorREM) {
    float flex = executive_sleep_flexibility_for_state(SLEEP_STATE_REM);
    EXPECT_FLOAT_EQ(flex, EXEC_SLEEP_FLEXIBILITY_REM);
    EXPECT_GT(flex, 0.0f) << "Some flexibility in REM (dream adaptation)";
}

/* Task Switch Cost Tests */
TEST_F(ExecutiveSleepHelperTest, SwitchCostAwake) {
    EXPECT_FLOAT_EQ(executive_sleep_switch_cost_for_state(SLEEP_STATE_AWAKE),
                    EXEC_SLEEP_SWITCH_COST_AWAKE);
}

TEST_F(ExecutiveSleepHelperTest, SwitchCostDrowsy) {
    float cost = executive_sleep_switch_cost_for_state(SLEEP_STATE_DROWSY);
    EXPECT_FLOAT_EQ(cost, EXEC_SLEEP_SWITCH_COST_DROWSY);
    EXPECT_GT(cost, 1.0f) << "Task switching slower when drowsy";
}

TEST_F(ExecutiveSleepHelperTest, SwitchCostNREM) {
    float cost = executive_sleep_switch_cost_for_state(SLEEP_STATE_LIGHT_NREM);
    EXPECT_FLOAT_EQ(cost, EXEC_SLEEP_SWITCH_COST_NREM);
    EXPECT_GT(cost, 5.0f) << "Task switching essentially blocked in NREM";
}

/* ============================================================================
 * Default Configuration Tests
 * ============================================================================ */

TEST_F(ExecutiveSleepHelperTest, DefaultConfigValid) {
    executive_sleep_config_t config;
    ASSERT_EQ(executive_sleep_default_config(&config), 0);

    EXPECT_TRUE(config.enable_inhibition_modulation);
    EXPECT_TRUE(config.enable_flexibility_modulation);
    EXPECT_TRUE(config.enable_switch_cost_modulation);
    EXPECT_GT(config.modulation_strength, 0.0f);
}

TEST_F(ExecutiveSleepHelperTest, DefaultConfigNullReturnsError) {
    EXPECT_EQ(executive_sleep_default_config(nullptr), -1);
}

/* ============================================================================
 * Biological Validity Tests
 * ============================================================================ */

TEST_F(ExecutiveSleepHelperTest, AwakeIsBaseline) {
    /* Awake state should be baseline (1.0) */
    EXPECT_FLOAT_EQ(executive_sleep_inhibition_for_state(SLEEP_STATE_AWAKE), 1.0f);
    EXPECT_FLOAT_EQ(executive_sleep_flexibility_for_state(SLEEP_STATE_AWAKE), 1.0f);
    EXPECT_FLOAT_EQ(executive_sleep_switch_cost_for_state(SLEEP_STATE_AWAKE), 1.0f);
}

TEST_F(ExecutiveSleepHelperTest, PFCSensitivityToSleep) {
    /* Prefrontal cortex is highly sensitive to sleep deprivation */
    float inhib_awake = executive_sleep_inhibition_for_state(SLEEP_STATE_AWAKE);
    float inhib_drowsy = executive_sleep_inhibition_for_state(SLEEP_STATE_DROWSY);

    /* Even mild drowsiness should significantly impair inhibition */
    EXPECT_LT(inhib_drowsy, inhib_awake * 0.7f) << "PFC sensitive to sleep loss";
}

TEST_F(ExecutiveSleepHelperTest, DreamBizarrenessFromReducedInhibition) {
    /* Reduced inhibition in REM explains dream bizarreness */
    float inhib_awake = executive_sleep_inhibition_for_state(SLEEP_STATE_AWAKE);
    float inhib_rem = executive_sleep_inhibition_for_state(SLEEP_STATE_REM);

    EXPECT_LT(inhib_rem, inhib_awake * 0.5f) << "Reduced inhibition in REM";
    EXPECT_GT(inhib_rem, 0.0f) << "But not completely absent";
}

TEST_F(ExecutiveSleepHelperTest, TaskSwitchingImpairment) {
    /* Task switching cost increases dramatically during sleep */
    float cost_awake = executive_sleep_switch_cost_for_state(SLEEP_STATE_AWAKE);
    float cost_drowsy = executive_sleep_switch_cost_for_state(SLEEP_STATE_DROWSY);
    float cost_nrem = executive_sleep_switch_cost_for_state(SLEEP_STATE_LIGHT_NREM);

    EXPECT_GT(cost_drowsy, cost_awake);
    EXPECT_GT(cost_nrem, cost_drowsy * 2) << "Switching essentially blocked in NREM";
}

TEST_F(ExecutiveSleepHelperTest, OfflineInNREM) {
    /* Executive functions should be offline during NREM */
    float inhib = executive_sleep_inhibition_for_state(SLEEP_STATE_DEEP_NREM);
    float flex = executive_sleep_flexibility_for_state(SLEEP_STATE_DEEP_NREM);

    EXPECT_FLOAT_EQ(inhib, 0.0f) << "No inhibition in deep NREM";
    EXPECT_FLOAT_EQ(flex, 0.0f) << "No flexibility in deep NREM";
}

TEST_F(ExecutiveSleepHelperTest, StroopDeficit) {
    /* Models Stroop deficits from sleep deprivation */
    float inhib_awake = executive_sleep_inhibition_for_state(SLEEP_STATE_AWAKE);
    float inhib_drowsy = executive_sleep_inhibition_for_state(SLEEP_STATE_DROWSY);

    /* Stroop relies on inhibition - drowsy should show significant deficit */
    float deficit = (inhib_awake - inhib_drowsy) / inhib_awake;
    EXPECT_GT(deficit, 0.3f) << "Significant inhibition deficit when drowsy";
}
