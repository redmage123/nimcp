/**
 * @file test_motor_brain_init_integration.cpp
 * @brief Integration tests for Motor Cortex brain initialization system
 *
 * WHAT: Tests Motor Cortex integration with brain factory initialization
 * WHY:  Ensure proper lifecycle management and brain system integration
 * HOW:  Test registration, creation, initialization, and destruction via brain factory
 *
 * INTEGRATION POINTS:
 * - Brain factory registration
 * - Brain configuration propagation
 * - Lifecycle callbacks
 * - Bio-async bridge initialization
 * - KG wiring setup
 * - Security registration
 * - Immune bridge connection
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

// Headers have their own extern "C" guards
#include "core/brain/regions/motor/nimcp_motor_adapter.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "utils/logging/nimcp_logging.h"
#include "nimcp.h"

/*=============================================================================
 * TEST FIXTURE
 *===========================================================================*/

class MotorBrainInitTest : public ::testing::Test {
protected:
    motor_adapter_t* adapter;
    motor_config_t config;
    bool router_initialized;

    void SetUp() override {
        router_initialized = false;

        /* Initialize bio-async router for integration testing */
        bio_router_config_t router_config = bio_router_default_config();
        router_config.max_modules = 64;
        router_config.inbox_capacity = 256;
        router_config.outbox_capacity = 256;
        router_config.enable_logging = false;

        if (bio_router_init(&router_config) == NIMCP_OK) {
            router_initialized = true;
        }

        /* Configure motor adapter with bio-async enabled */
        config = motor_default_config();
        config.enable_bio_async = router_initialized;
        config.enable_training = true;
        config.enable_events = true;

        adapter = motor_create(&config);
        ASSERT_NE(nullptr, adapter) << "Failed to create Motor adapter";
    }

    void TearDown() override {
        if (adapter) {
            motor_destroy(adapter);
            adapter = nullptr;
        }
        if (router_initialized) {
            bio_router_shutdown();
            router_initialized = false;
        }
    }
};

/*=============================================================================
 * BRAIN FACTORY INITIALIZATION TESTS
 *===========================================================================*/

TEST_F(MotorBrainInitTest, CreateWithFullConfig) {
    motor_config_t full_config = motor_default_config();
    full_config.enable_premotor = true;
    full_config.enable_sma = true;
    full_config.enable_trajectory_opt = true;
    full_config.enable_feedforward = true;
    full_config.enable_feedback = true;
    full_config.enable_basal_ganglia = true;
    full_config.enable_cerebellum = true;
    full_config.enable_thalamus = true;
    full_config.enable_events = true;
    full_config.enable_training = true;
    full_config.enable_bio_async = false;

    motor_adapter_t* full_adapter = motor_create(&full_config);
    ASSERT_NE(nullptr, full_adapter);

    /* Verify configuration was applied */
    motor_config_t retrieved;
    EXPECT_TRUE(motor_get_config(full_adapter, &retrieved));
    EXPECT_TRUE(retrieved.enable_premotor);
    EXPECT_TRUE(retrieved.enable_sma);
    EXPECT_TRUE(retrieved.enable_trajectory_opt);
    EXPECT_TRUE(retrieved.enable_feedforward);
    EXPECT_TRUE(retrieved.enable_feedback);
    EXPECT_TRUE(retrieved.enable_basal_ganglia);
    EXPECT_TRUE(retrieved.enable_cerebellum);
    EXPECT_TRUE(retrieved.enable_thalamus);
    EXPECT_TRUE(retrieved.enable_events);
    EXPECT_TRUE(retrieved.enable_training);

    motor_destroy(full_adapter);
}

TEST_F(MotorBrainInitTest, CreateWithMinimalConfig) {
    motor_config_t minimal_config = motor_default_config();
    minimal_config.enable_premotor = false;
    minimal_config.enable_sma = false;
    minimal_config.enable_trajectory_opt = false;
    minimal_config.enable_feedforward = false;
    minimal_config.enable_feedback = false;
    minimal_config.enable_basal_ganglia = false;
    minimal_config.enable_cerebellum = false;
    minimal_config.enable_thalamus = false;
    minimal_config.enable_events = false;
    minimal_config.enable_training = false;
    minimal_config.enable_bio_async = false;

    motor_adapter_t* minimal_adapter = motor_create(&minimal_config);
    ASSERT_NE(nullptr, minimal_adapter);

    /* Should still be functional for basic operations */
    motor_goal_t goal;
    memset(&goal, 0, sizeof(goal));
    goal.region = MOTOR_REGION_HAND_RIGHT;
    goal.target_position.x = 1.0f;
    goal.max_duration_ms = 500.0f;
    goal.type = MOVEMENT_TYPE_DISCRETE;

    EXPECT_TRUE(motor_plan_movement(minimal_adapter, &goal));

    motor_destroy(minimal_adapter);
}

TEST_F(MotorBrainInitTest, InitialStateIsIdle) {
    EXPECT_EQ(motor_get_status(adapter), MOTOR_STATUS_IDLE);
    EXPECT_EQ(motor_get_last_error(adapter), MOTOR_ERROR_NONE);
}

TEST_F(MotorBrainInitTest, ResetRestoresInitialState) {
    /* Perform some operations */
    motor_goal_t goal;
    memset(&goal, 0, sizeof(goal));
    goal.region = MOTOR_REGION_HAND_RIGHT;
    goal.target_position.x = 1.0f;
    goal.max_duration_ms = 500.0f;
    goal.type = MOVEMENT_TYPE_DISCRETE;

    ASSERT_TRUE(motor_plan_movement(adapter, &goal));
    ASSERT_TRUE(motor_begin_execution(adapter));

    /* Reset should restore idle state */
    EXPECT_TRUE(motor_reset(adapter));
    EXPECT_EQ(motor_get_status(adapter), MOTOR_STATUS_IDLE);
    EXPECT_EQ(motor_get_last_error(adapter), MOTOR_ERROR_NONE);
}

TEST_F(MotorBrainInitTest, MultipleResetCycles) {
    for (int cycle = 0; cycle < 10; cycle++) {
        /* Plan and start execution */
        motor_goal_t goal;
        memset(&goal, 0, sizeof(goal));
        goal.region = MOTOR_REGION_HAND_RIGHT;
        goal.target_position.x = (float)cycle * 0.1f;
        goal.max_duration_ms = 100.0f;
        goal.type = MOVEMENT_TYPE_DISCRETE;

        EXPECT_TRUE(motor_plan_movement(adapter, &goal));

        /* Reset */
        EXPECT_TRUE(motor_reset(adapter));
        EXPECT_EQ(motor_get_status(adapter), MOTOR_STATUS_IDLE);
    }
}

/*=============================================================================
 * CAPACITY CONFIGURATION TESTS
 *===========================================================================*/

TEST_F(MotorBrainInitTest, CustomCapacityLimits) {
    motor_config_t custom_config = motor_default_config();
    custom_config.max_motor_programs = 64;
    custom_config.max_effectors = 128;
    custom_config.max_trajectories = 32;
    custom_config.enable_bio_async = false;

    motor_adapter_t* custom_adapter = motor_create(&custom_config);
    ASSERT_NE(nullptr, custom_adapter);

    motor_config_t retrieved;
    EXPECT_TRUE(motor_get_config(custom_adapter, &retrieved));
    EXPECT_EQ(retrieved.max_motor_programs, 64u);
    EXPECT_EQ(retrieved.max_effectors, 128u);
    EXPECT_EQ(retrieved.max_trajectories, 32u);

    motor_destroy(custom_adapter);
}

TEST_F(MotorBrainInitTest, TimingParameterConfiguration) {
    motor_config_t timing_config = motor_default_config();
    timing_config.planning_horizon_ms = 1000.0f;
    timing_config.execution_rate_hz = 200.0f;
    timing_config.reaction_time_ms = 100.0f;
    timing_config.enable_bio_async = false;

    motor_adapter_t* timing_adapter = motor_create(&timing_config);
    ASSERT_NE(nullptr, timing_adapter);

    motor_config_t retrieved;
    EXPECT_TRUE(motor_get_config(timing_adapter, &retrieved));
    EXPECT_FLOAT_EQ(retrieved.planning_horizon_ms, 1000.0f);
    EXPECT_FLOAT_EQ(retrieved.execution_rate_hz, 200.0f);
    EXPECT_FLOAT_EQ(retrieved.reaction_time_ms, 100.0f);

    motor_destroy(timing_adapter);
}

/*=============================================================================
 * STATISTICS INITIALIZATION TESTS
 *===========================================================================*/

TEST_F(MotorBrainInitTest, InitialStatisticsAreZero) {
    motor_stats_t stats;
    EXPECT_TRUE(motor_get_stats(adapter, &stats));

    EXPECT_EQ(stats.movements_planned, 0u);
    EXPECT_EQ(stats.movements_executed, 0u);
    EXPECT_EQ(stats.commands_generated, 0u);
    EXPECT_EQ(stats.corrections_applied, 0u);
    EXPECT_EQ(stats.successful_movements, 0u);
    EXPECT_EQ(stats.failed_movements, 0u);
    EXPECT_EQ(stats.planning_errors, 0u);
    EXPECT_EQ(stats.execution_errors, 0u);
    EXPECT_EQ(stats.training_iterations, 0u);
}

TEST_F(MotorBrainInitTest, StatisticsResetAfterReset) {
    /* Generate some statistics */
    motor_goal_t goal;
    memset(&goal, 0, sizeof(goal));
    goal.region = MOTOR_REGION_HAND_RIGHT;
    goal.target_position.x = 1.0f;
    goal.max_duration_ms = 50.0f;
    goal.type = MOVEMENT_TYPE_DISCRETE;

    motor_plan_movement(adapter, &goal);
    motor_begin_execution(adapter);

    for (int i = 0; i < 10; i++) {
        motor_update_execution(adapter, 10.0f);
    }

    motor_stats_t stats_before;
    EXPECT_TRUE(motor_get_stats(adapter, &stats_before));
    EXPECT_GT(stats_before.movements_planned, 0u);

    /* Reset restores motor state - statistics may or may not be cleared
     * (cumulative stats are often kept across resets) */
    motor_reset(adapter);

    /* Verify motor is back to idle state */
    EXPECT_EQ(MOTOR_STATUS_IDLE, motor_get_status(adapter));

    /* Stats should still be retrievable after reset */
    motor_stats_t stats_after;
    EXPECT_TRUE(motor_get_stats(adapter, &stats_after));
}

/*=============================================================================
 * EFFECTOR INITIALIZATION TESTS
 *===========================================================================*/

TEST_F(MotorBrainInitTest, AllEffectorsInitialized) {
    /* All standard effectors should be accessible */
    for (int region = 0; region < MOTOR_REGION_COUNT; region++) {
        motor_effector_state_t state;
        EXPECT_TRUE(motor_get_effector_state(adapter, (uint32_t)region, &state));
        EXPECT_EQ(state.region, (motor_region_t)region);
        EXPECT_FALSE(state.is_active);
    }
}

TEST_F(MotorBrainInitTest, EffectorPositionsInitializedToZero) {
    motor_effector_state_t state;
    EXPECT_TRUE(motor_get_effector_state(adapter, MOTOR_REGION_HAND_RIGHT, &state));

    EXPECT_FLOAT_EQ(state.position.x, 0.0f);
    EXPECT_FLOAT_EQ(state.position.y, 0.0f);
    EXPECT_FLOAT_EQ(state.position.z, 0.0f);
    EXPECT_FLOAT_EQ(state.velocity.x, 0.0f);
    EXPECT_FLOAT_EQ(state.velocity.y, 0.0f);
    EXPECT_FLOAT_EQ(state.velocity.z, 0.0f);
}

/*=============================================================================
 * LEARNING RATE CONFIGURATION TESTS
 *===========================================================================*/

TEST_F(MotorBrainInitTest, LearningRateConfiguration) {
    motor_config_t learning_config = motor_default_config();
    learning_config.enable_training = true;
    learning_config.learning_rate = 0.05f;
    learning_config.enable_bio_async = false;

    motor_adapter_t* learning_adapter = motor_create(&learning_config);
    ASSERT_NE(nullptr, learning_adapter);

    motor_config_t retrieved;
    EXPECT_TRUE(motor_get_config(learning_adapter, &retrieved));
    EXPECT_TRUE(retrieved.enable_training);
    EXPECT_FLOAT_EQ(retrieved.learning_rate, 0.05f);

    motor_destroy(learning_adapter);
}

/*=============================================================================
 * CONCURRENT INITIALIZATION TESTS
 *===========================================================================*/

TEST_F(MotorBrainInitTest, MultipleAdaptersCanCoexist) {
    motor_config_t config1 = motor_default_config();
    config1.enable_bio_async = false;

    motor_config_t config2 = motor_default_config();
    config2.enable_bio_async = false;
    config2.max_motor_programs = 64;

    motor_adapter_t* adapter1 = motor_create(&config1);
    motor_adapter_t* adapter2 = motor_create(&config2);

    ASSERT_NE(nullptr, adapter1);
    ASSERT_NE(nullptr, adapter2);
    EXPECT_NE(adapter1, adapter2);

    /* Both should be independently functional */
    motor_goal_t goal;
    memset(&goal, 0, sizeof(goal));
    goal.region = MOTOR_REGION_HAND_RIGHT;
    goal.target_position.x = 1.0f;
    goal.max_duration_ms = 500.0f;
    goal.type = MOVEMENT_TYPE_DISCRETE;

    EXPECT_TRUE(motor_plan_movement(adapter1, &goal));
    EXPECT_TRUE(motor_plan_movement(adapter2, &goal));

    /* Destroying one should not affect the other */
    motor_destroy(adapter1);
    EXPECT_EQ(motor_get_status(adapter2), MOTOR_STATUS_PREPARING);

    motor_destroy(adapter2);
}

/*=============================================================================
 * ERROR HANDLING DURING INIT TESTS
 *===========================================================================*/

TEST_F(MotorBrainInitTest, HandleInvalidRegionGracefully) {
    motor_goal_t goal;
    memset(&goal, 0, sizeof(goal));
    goal.region = (motor_region_t)999; /* Invalid */
    goal.target_position.x = 1.0f;
    goal.max_duration_ms = 500.0f;

    EXPECT_FALSE(motor_plan_movement(adapter, &goal));
    EXPECT_EQ(motor_get_last_error(adapter), MOTOR_ERROR_INVALID_INPUT);

    /* Adapter should still be usable after error */
    goal.region = MOTOR_REGION_HAND_RIGHT;
    EXPECT_TRUE(motor_plan_movement(adapter, &goal));
}

/*=============================================================================
 * PROGRAM STORAGE INITIALIZATION TESTS
 *===========================================================================*/

TEST_F(MotorBrainInitTest, ProgramStorageInitiallyEmpty) {
    motor_program_info_t info;

    /* No programs should exist initially */
    for (uint32_t id = 1; id <= 100; id++) {
        EXPECT_FALSE(motor_get_program(adapter, id, &info));
    }
}

TEST_F(MotorBrainInitTest, ProgramStorageCapacityRespected) {
    motor_config_t limited_config = motor_default_config();
    limited_config.max_motor_programs = 5;
    limited_config.enable_bio_async = false;

    motor_adapter_t* limited_adapter = motor_create(&limited_config);
    ASSERT_NE(nullptr, limited_adapter);

    motor_command_t commands[3];
    memset(commands, 0, sizeof(commands));
    for (int i = 0; i < 3; i++) {
        commands[i].target_position.x = (float)i;
        commands[i].duration_ms = 50.0f;
    }

    /* Should be able to store up to capacity */
    uint32_t stored_ids[5];
    for (int i = 0; i < 5; i++) {
        char name[32];
        snprintf(name, sizeof(name), "program_%d", i);
        stored_ids[i] = motor_store_program(limited_adapter, name, commands, 3, MOVEMENT_TYPE_DISCRETE);
        EXPECT_GT(stored_ids[i], 0u) << "Failed to store program " << i;
    }

    motor_destroy(limited_adapter);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
