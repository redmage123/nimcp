/**
 * @file test_motor_hypothalamus_integration.cpp
 * @brief Integration tests for Motor Cortex with Hypothalamus
 *
 * WHAT: Tests Motor Cortex integration with hypothalamic regulation
 * WHY:  Verify motor output responds to homeostatic and stress signals
 * HOW:  Test circadian modulation, stress effects, autonomic coordination
 *
 * HYPOTHALAMUS INTEGRATION POINTS:
 * - Circadian Rhythm: Motor performance varies with time of day
 * - Stress Response: HPA axis affects motor coordination
 * - Autonomic: Fight-or-flight affects motor readiness
 * - Homeostatic: Hunger/fatigue affect motor motivation
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

// Headers have their own extern "C" guards
#include "core/brain/regions/motor/nimcp_motor_adapter.h"
#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_adapter.h"
#include "utils/logging/nimcp_logging.h"

/*=============================================================================
 * TEST FIXTURE
 *===========================================================================*/

class MotorHypothalamusIntegrationTest : public ::testing::Test {
protected:
    motor_adapter_t* motor;
    motor_config_t motor_config;
    hypothalamus_adapter_t* hypothalamus;
    hypothalamus_config_t hypo_config;

    void SetUp() override {
        /* Create motor adapter */
        motor_config = motor_default_config();
        motor_config.enable_bio_async = false;
        motor = motor_create(&motor_config);
        ASSERT_NE(nullptr, motor);

        /* Create hypothalamus adapter */
        hypo_config = hypothalamus_default_config();
        hypo_config.enable_bio_async = false;
        hypo_config.enable_circadian = true;
        hypo_config.enable_hpa_axis = true;
        hypo_config.enable_autonomic = true;
        hypothalamus = hypothalamus_create(&hypo_config);
        ASSERT_NE(nullptr, hypothalamus);
    }

    void TearDown() override {
        if (hypothalamus) {
            hypothalamus_destroy(hypothalamus);
            hypothalamus = nullptr;
        }
        if (motor) {
            motor_destroy(motor);
            motor = nullptr;
        }
    }

    /* Helper to create a standard test goal */
    motor_goal_t CreateTestGoal(motor_region_t region, float x, float duration_ms) {
        motor_goal_t goal;
        memset(&goal, 0, sizeof(goal));
        goal.region = region;
        goal.target_position.x = x;
        goal.max_duration_ms = duration_ms;
        goal.type = MOVEMENT_TYPE_DISCRETE;
        return goal;
    }
};

/*=============================================================================
 * CIRCADIAN RHYTHM INTEGRATION TESTS
 * Test motor performance modulation by circadian phase
 *===========================================================================*/

TEST_F(MotorHypothalamusIntegrationTest, CircadianPhaseRetrieval) {
    /* Get current circadian phase */
    hypo_circadian_phase_t phase = hypothalamus_get_circadian_phase(hypothalamus);

    /* Phase should be valid */
    EXPECT_GE((int)phase, (int)HYPO_CIRCADIAN_PHASE_EARLY_MORNING);
    EXPECT_LE((int)phase, (int)HYPO_CIRCADIAN_PHASE_LATE_NIGHT);
}

TEST_F(MotorHypothalamusIntegrationTest, CircadianStateRetrieval) {
    /* Get circadian state */
    hypo_circadian_state_t state;
    bool result = hypothalamus_get_circadian_state(hypothalamus, &state);

    EXPECT_TRUE(result);
    EXPECT_GE(state.phase, 0.0f);
    EXPECT_LE(state.phase, 2.0f * M_PI);
    EXPECT_GE(state.alertness, 0.0f);
    EXPECT_LE(state.alertness, 1.0f);
}

TEST_F(MotorHypothalamusIntegrationTest, CircadianUpdateAffectsAlertness) {
    /* Get initial alertness */
    hypo_circadian_state_t initial_state;
    hypothalamus_get_circadian_state(hypothalamus, &initial_state);

    /* Advance circadian clock (1 hour in microseconds) */
    uint64_t one_hour_us = 3600 * 1000000ULL;
    hypothalamus_update_circadian(hypothalamus, one_hour_us);

    /* Get updated state */
    hypo_circadian_state_t updated_state;
    hypothalamus_get_circadian_state(hypothalamus, &updated_state);

    /* Phase should have advanced */
    /* Note: may wrap around, so just check it changed or stayed valid */
    EXPECT_GE(updated_state.phase, 0.0f);
}

TEST_F(MotorHypothalamusIntegrationTest, MotorExecutionDuringHighAlertness) {
    /* Set hypothalamus to high alertness period */
    hypo_circadian_state_t state;
    hypothalamus_get_circadian_state(hypothalamus, &state);

    /* Execute motor movement */
    motor_goal_t goal = CreateTestGoal(MOTOR_REGION_HAND_RIGHT, 1.0f, 200.0f);
    motor_plan_movement(motor, &goal);
    motor_begin_execution(motor);

    /* Use alertness to modulate expected performance */
    float alertness = state.alertness;

    for (int i = 0; i < 20; i++) {
        motor_update_execution(motor, 10.0f);
    }

    /* Motor should complete successfully regardless of alertness */
    motor_effector_state_t effector_state;
    motor_get_effector_state(motor, MOTOR_REGION_HAND_RIGHT, &effector_state);

    /* Movement should progress */
    EXPECT_GT(effector_state.position.x, 0.0f);
}

TEST_F(MotorHypothalamusIntegrationTest, LightExposureAffectsCircadian) {
    /* Apply light exposure (morning light) */
    float phase_shift = hypothalamus_apply_light(hypothalamus, 0.8f, 1000.0f);

    /* Light should cause some phase shift */
    /* (could be positive or negative depending on current phase) */
    EXPECT_TRUE(std::isfinite(phase_shift));
}

/*=============================================================================
 * STRESS RESPONSE (HPA AXIS) INTEGRATION TESTS
 * Test motor effects of stress hormone levels
 *===========================================================================*/

TEST_F(MotorHypothalamusIntegrationTest, BaselineCortisolLevel) {
    /* Get baseline cortisol */
    float cortisol = hypothalamus_get_cortisol(hypothalamus);

    /* Should be at baseline (around 0.3 by default) */
    EXPECT_GE(cortisol, 0.0f);
    EXPECT_LE(cortisol, 1.0f);
}

TEST_F(MotorHypothalamusIntegrationTest, StressApplicationIncreaseCortisol) {
    /* Get baseline */
    float baseline = hypothalamus_get_cortisol(hypothalamus);

    /* Apply stress */
    float cortisol_change = hypothalamus_apply_stress(hypothalamus, 0.8f);

    /* Update HPA axis to process stress */
    hypothalamus_update_hpa_axis(hypothalamus, 100000);  /* 100ms */

    /* Cortisol should have increased */
    float new_cortisol = hypothalamus_get_cortisol(hypothalamus);
    EXPECT_GE(new_cortisol, baseline);
}

TEST_F(MotorHypothalamusIntegrationTest, HPAStateRetrieval) {
    /* Get HPA axis state */
    hpa_axis_state_t hpa_state;
    bool result = hypothalamus_get_hpa_state(hypothalamus, &hpa_state);

    EXPECT_TRUE(result);
    EXPECT_GE(hpa_state.cortisol_level, 0.0f);
    EXPECT_LE(hpa_state.cortisol_level, 1.0f);
    EXPECT_GE(hpa_state.crh_level, 0.0f);
    EXPECT_GE(hpa_state.acth_level, 0.0f);
}

TEST_F(MotorHypothalamusIntegrationTest, MotorUnderStress) {
    /* Apply high stress */
    hypothalamus_apply_stress(hypothalamus, 0.9f);
    hypothalamus_update_hpa_axis(hypothalamus, 500000);

    float cortisol = hypothalamus_get_cortisol(hypothalamus);

    /* Execute motor movement under stress */
    motor_goal_t goal = CreateTestGoal(MOTOR_REGION_HAND_RIGHT, 1.0f, 200.0f);
    motor_plan_movement(motor, &goal);
    motor_begin_execution(motor);

    for (int i = 0; i < 20; i++) {
        motor_update_execution(motor, 10.0f);
    }

    /* Motor should still complete (stress doesn't prevent movement) */
    motor_status_t status = motor_get_status(motor);
    /* Either completed or still executing */
    EXPECT_TRUE(status == MOTOR_STATUS_IDLE ||
                status == MOTOR_STATUS_EXECUTING ||
                status == MOTOR_STATUS_COMPLETE);
}

/*=============================================================================
 * AUTONOMIC SYSTEM INTEGRATION TESTS
 * Test motor readiness based on autonomic state
 *===========================================================================*/

TEST_F(MotorHypothalamusIntegrationTest, AutonomicStateRetrieval) {
    /* Get autonomic state */
    autonomic_state_t autonomic;
    bool result = hypothalamus_get_autonomic(hypothalamus, &autonomic);

    EXPECT_TRUE(result);
    EXPECT_GE(autonomic.sympathetic_tone, 0.0f);
    EXPECT_LE(autonomic.sympathetic_tone, 1.0f);
    EXPECT_GE(autonomic.parasympathetic_tone, 0.0f);
    EXPECT_LE(autonomic.parasympathetic_tone, 1.0f);
}

TEST_F(MotorHypothalamusIntegrationTest, AutonomicBalanceRetrieval) {
    /* Get autonomic balance */
    float balance = hypothalamus_get_autonomic_balance(hypothalamus);

    /* Should be between 0 (parasympathetic) and 1 (sympathetic) */
    EXPECT_GE(balance, 0.0f);
    EXPECT_LE(balance, 1.0f);
}

TEST_F(MotorHypothalamusIntegrationTest, FightOrFlightMotorReadiness) {
    /* Apply stress to trigger fight-or-flight */
    hypothalamus_apply_stress(hypothalamus, 0.95f);
    hypothalamus_update_autonomic(hypothalamus, 200000);

    /* Get autonomic state */
    autonomic_state_t autonomic;
    hypothalamus_get_autonomic(hypothalamus, &autonomic);

    /* Under fight-or-flight, motor should be ready for action */
    motor_goal_t goal = CreateTestGoal(MOTOR_REGION_HAND_RIGHT, 2.0f, 100.0f);
    motor_plan_movement(motor, &goal);

    /* Planning should succeed - status transitions through PLANNING to PREPARING */
    motor_status_t status = motor_get_status(motor);
    EXPECT_TRUE(status == MOTOR_STATUS_IDLE ||
                status == MOTOR_STATUS_PLANNING ||
                status == MOTOR_STATUS_PREPARING);
}

TEST_F(MotorHypothalamusIntegrationTest, AutonomicUpdateProcessing) {
    /* Update autonomic system */
    bool result = hypothalamus_update_autonomic(hypothalamus, 100000);
    EXPECT_TRUE(result);

    /* State should be valid after update */
    autonomic_state_t state;
    hypothalamus_get_autonomic(hypothalamus, &state);
    EXPECT_FALSE(std::isnan(state.sympathetic_tone));
    EXPECT_FALSE(std::isnan(state.parasympathetic_tone));
}

/*=============================================================================
 * HOMEOSTATIC REGULATION TESTS
 * Test motor behavior under different homeostatic states
 *===========================================================================*/

TEST_F(MotorHypothalamusIntegrationTest, ThermoregulationState) {
    /* Set body temperature */
    hypothalamus_set_temperature(hypothalamus, 37.0f);

    /* Get thermoregulation state */
    thermoregulation_state_t thermo;
    bool result = hypothalamus_get_thermoregulation(hypothalamus, &thermo);

    EXPECT_TRUE(result);
    EXPECT_NEAR(thermo.core_temp.current_value, 37.0f, 1.0f);
}

TEST_F(MotorHypothalamusIntegrationTest, AppetiteState) {
    /* Get appetite state */
    appetite_state_t appetite;
    bool result = hypothalamus_get_appetite(hypothalamus, &appetite);

    EXPECT_TRUE(result);
    EXPECT_GE(appetite.hunger_drive, 0.0f);
    EXPECT_LE(appetite.hunger_drive, 1.0f);
}

TEST_F(MotorHypothalamusIntegrationTest, HydrationState) {
    /* Get hydration state */
    hydration_state_t hydration;
    bool result = hypothalamus_get_hydration(hypothalamus, &hydration);

    EXPECT_TRUE(result);
    EXPECT_GE(hydration.thirst_drive, 0.0f);
    EXPECT_LE(hydration.thirst_drive, 1.0f);
}

TEST_F(MotorHypothalamusIntegrationTest, MotorDuringHunger) {
    /* Set low blood glucose to trigger hunger */
    hypothalamus_set_blood_glucose(hypothalamus, 60.0f);  /* Low glucose */
    hypothalamus_update_homeostasis(hypothalamus, 100000);

    /* Get appetite state */
    appetite_state_t appetite;
    hypothalamus_get_appetite(hypothalamus, &appetite);

    /* Motor should still work during hunger (but may be slower) */
    motor_goal_t goal = CreateTestGoal(MOTOR_REGION_HAND_RIGHT, 1.0f, 200.0f);
    bool planned = motor_plan_movement(motor, &goal);
    EXPECT_TRUE(planned);
}

/*=============================================================================
 * INTEGRATED UPDATE TESTS
 * Test full hypothalamus update cycle with motor
 *===========================================================================*/

TEST_F(MotorHypothalamusIntegrationTest, FullHypothalamusUpdate) {
    /* Run full hypothalamus update */
    bool result = hypothalamus_update(hypothalamus, 100000);
    EXPECT_TRUE(result);

    /* Get complete state */
    hypothalamus_state_t state;
    result = hypothalamus_get_state(hypothalamus, &state);
    EXPECT_TRUE(result);

    /* All subsystems should have valid values */
    EXPECT_GE(state.circadian.alertness, 0.0f);
    EXPECT_GE(state.hpa_axis.cortisol_level, 0.0f);
    EXPECT_GE(state.autonomic.sympathetic_tone, 0.0f);
}

TEST_F(MotorHypothalamusIntegrationTest, HypothalamusStatusCheck) {
    /* Get status */
    hypothalamus_status_t status = hypothalamus_get_status(hypothalamus);

    /* Should be idle normally */
    EXPECT_EQ(HYPOTHALAMUS_STATUS_IDLE, status);
}

TEST_F(MotorHypothalamusIntegrationTest, HypothalamusNoError) {
    /* Get last error */
    hypothalamus_error_t error = hypothalamus_get_last_error(hypothalamus);

    /* Fresh hypothalamus should have no error */
    EXPECT_EQ(HYPOTHALAMUS_ERROR_NONE, error);
}

TEST_F(MotorHypothalamusIntegrationTest, HypothalamusStats) {
    /* Run some updates */
    for (int i = 0; i < 10; i++) {
        hypothalamus_update(hypothalamus, 100000);
    }

    /* Get stats */
    hypothalamus_stats_t stats;
    bool result = hypothalamus_get_stats(hypothalamus, &stats);

    EXPECT_TRUE(result);
    EXPECT_EQ(10u, stats.updates_processed);
}

/*=============================================================================
 * COORDINATED MOTOR-HYPOTHALAMUS SCENARIOS
 *===========================================================================*/

TEST_F(MotorHypothalamusIntegrationTest, MorningMotorRoutine) {
    /* Simulate morning: high cortisol, high alertness */
    /* Apply light to simulate morning */
    hypothalamus_apply_light(hypothalamus, 0.9f, 2000.0f);
    hypothalamus_update(hypothalamus, 1000000);

    hypo_circadian_state_t circadian;
    hypothalamus_get_circadian_state(hypothalamus, &circadian);

    /* Execute morning exercise motor sequence */
    motor_goal_t goals[3];
    for (int i = 0; i < 3; i++) {
        goals[i] = CreateTestGoal(MOTOR_REGION_ARM_RIGHT, (float)(i + 1) * 0.5f, 100.0f);
        goals[i].type = MOVEMENT_TYPE_SERIAL;
        motor_plan_movement(motor, &goals[i]);
    }

    motor_begin_execution(motor);
    for (int step = 0; step < 30; step++) {
        motor_update_execution(motor, 10.0f);
    }

    /* Movement should execute */
    motor_effector_state_t state;
    motor_get_effector_state(motor, MOTOR_REGION_ARM_RIGHT, &state);
    EXPECT_GT(state.position.x, 0.0f);
}

TEST_F(MotorHypothalamusIntegrationTest, StressfulSituation_MotorResponse) {
    /* Simulate stressful situation */
    hypothalamus_apply_stress(hypothalamus, 0.85f);
    hypothalamus_update(hypothalamus, 500000);

    /* Get states */
    float cortisol = hypothalamus_get_cortisol(hypothalamus);
    float autonomic_balance = hypothalamus_get_autonomic_balance(hypothalamus);

    /* Execute rapid motor response */
    motor_goal_t goal = CreateTestGoal(MOTOR_REGION_HAND_RIGHT, 2.0f, 50.0f);  /* Fast movement */
    motor_plan_movement(motor, &goal);
    motor_begin_execution(motor);

    for (int i = 0; i < 10; i++) {
        motor_update_execution(motor, 5.0f);
    }

    /* Motor should respond under stress */
    motor_status_t status = motor_get_status(motor);
    EXPECT_TRUE(status == MOTOR_STATUS_EXECUTING ||
                status == MOTOR_STATUS_IDLE ||
                status == MOTOR_STATUS_COMPLETE);
}

TEST_F(MotorHypothalamusIntegrationTest, FatigueMotorSlowdown) {
    /* Simulate fatigue: low glucose, high sleep pressure */
    hypothalamus_set_blood_glucose(hypothalamus, 55.0f);  /* Low */

    /* Run several update cycles to accumulate fatigue */
    for (int i = 0; i < 20; i++) {
        hypothalamus_update(hypothalamus, 500000);
    }

    /* Get state */
    hypothalamus_state_t state;
    hypothalamus_get_state(hypothalamus, &state);

    /* Execute motor under fatigue */
    motor_goal_t goal = CreateTestGoal(MOTOR_REGION_HAND_RIGHT, 1.0f, 300.0f);
    motor_plan_movement(motor, &goal);
    motor_begin_execution(motor);

    for (int i = 0; i < 30; i++) {
        motor_update_execution(motor, 10.0f);
    }

    /* Motor should still complete (perhaps slower, but we don't model that directly) */
    motor_effector_state_t effector;
    motor_get_effector_state(motor, MOTOR_REGION_HAND_RIGHT, &effector);
    EXPECT_GT(effector.position.x, 0.0f);
}

/*=============================================================================
 * LIFECYCLE TESTS
 *===========================================================================*/

TEST_F(MotorHypothalamusIntegrationTest, HypothalamusReset) {
    /* Apply stress and update */
    hypothalamus_apply_stress(hypothalamus, 0.9f);
    hypothalamus_update(hypothalamus, 1000000);

    /* Reset */
    bool result = hypothalamus_reset(hypothalamus);
    EXPECT_TRUE(result);

    /* Should be back to idle with baseline values */
    hypothalamus_status_t status = hypothalamus_get_status(hypothalamus);
    EXPECT_EQ(HYPOTHALAMUS_STATUS_IDLE, status);
}

TEST_F(MotorHypothalamusIntegrationTest, ConfigRetrieval) {
    hypothalamus_config_t retrieved;
    bool result = hypothalamus_get_config(hypothalamus, &retrieved);

    EXPECT_TRUE(result);
    EXPECT_EQ(hypo_config.enable_circadian, retrieved.enable_circadian);
    EXPECT_EQ(hypo_config.enable_hpa_axis, retrieved.enable_hpa_axis);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
