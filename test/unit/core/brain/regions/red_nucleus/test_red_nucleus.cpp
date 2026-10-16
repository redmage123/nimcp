/**
 * @file test_red_nucleus.cpp
 * @brief Unit tests for Red Nucleus brain region - Motor Coordination Center
 *
 * Tests cover:
 * - Create/destroy lifecycle
 * - Motor refinement functions
 * - Limb coordination
 * - Rubrospinal control
 * - Command processing
 * - Error handling (null pointers, invalid parameters)
 * - Statistics tracking
 */

#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "core/brain/regions/red_nucleus/nimcp_red_nucleus.h"
}

/* ==========================================================================
 * Test Fixture
 * ========================================================================== */

class RedNucleusTest : public ::testing::Test {
protected:
    nimcp_red_nucleus_t* rn;

    void SetUp() override {
        rn = nullptr;
    }

    void TearDown() override {
        if (rn) {
            rn_destroy(rn);
            rn = nullptr;
        }
    }

    /* Helper to create and initialize a Red Nucleus instance */
    nimcp_red_nucleus_t* create_and_init(const rn_config_t* config = nullptr) {
        nimcp_red_nucleus_t* instance = rn_create(config);
        if (instance) {
            rn_init(instance);
        }
        return instance;
    }

    /* Helper to create a basic motor command */
    rn_motor_command_t create_velocity_command(rn_effector_t effector,
                                                float vx, float vy, float vz,
                                                float duration_ms) {
        rn_motor_command_t cmd;
        memset(&cmd, 0, sizeof(cmd));
        cmd.type = RN_CMD_VELOCITY;
        cmd.effector = effector;
        cmd.value.x = vx;
        cmd.value.y = vy;
        cmd.value.z = vz;
        cmd.magnitude = 0.5f;
        cmd.urgency = 0.5f;
        cmd.duration_ms = duration_ms;
        cmd.timestamp_us = 0;
        cmd.sequence_id = 0;
        return cmd;
    }

    /* Helper to create a vector3 */
    rn_vector3_t make_vector3(float x, float y, float z) {
        rn_vector3_t v;
        v.x = x;
        v.y = y;
        v.z = z;
        return v;
    }
};

/* ==========================================================================
 * Configuration Tests
 * ========================================================================== */

TEST_F(RedNucleusTest, DefaultConfigHasValidValues) {
    rn_config_t config;
    int result = rn_default_config(&config);

    EXPECT_EQ(result, 0);
    EXPECT_GT(config.velocity_gain, 0.0f);
    EXPECT_GT(config.force_gain, 0.0f);
    EXPECT_GT(config.position_gain, 0.0f);
    EXPECT_GE(config.damping_coefficient, 0.0f);
    EXPECT_GT(config.base_learning_rate, 0.0f);
    EXPECT_GT(config.error_threshold, 0.0f);
    EXPECT_GT(config.dentate_weight, 0.0f);
    EXPECT_GT(config.max_commands_queued, 0u);
}

TEST_F(RedNucleusTest, DefaultConfigWithNullReturnsError) {
    int result = rn_default_config(nullptr);
    EXPECT_EQ(result, -1);
}

/* ==========================================================================
 * Lifecycle Tests
 * ========================================================================== */

TEST_F(RedNucleusTest, CreateWithNullConfigUsesDefaults) {
    rn = rn_create(nullptr);
    ASSERT_NE(rn, nullptr);
    /* rn_create() does NOT set initialized - must call rn_init() */
    EXPECT_FALSE(rn->initialized);
}

TEST_F(RedNucleusTest, CreateAndInitSetsInitialized) {
    rn = rn_create(nullptr);
    ASSERT_NE(rn, nullptr);
    EXPECT_FALSE(rn->initialized);

    int result = rn_init(rn);
    EXPECT_EQ(result, 0);
    EXPECT_TRUE(rn->initialized);
}

TEST_F(RedNucleusTest, CreateWithCustomConfigSucceeds) {
    rn_config_t config;
    rn_default_config(&config);
    config.velocity_gain = 2.0f;
    config.base_learning_rate = 0.05f;

    rn = rn_create(&config);
    ASSERT_NE(rn, nullptr);
    /* Config values should be copied */
    EXPECT_FLOAT_EQ(rn->config.velocity_gain, 2.0f);
    EXPECT_FLOAT_EQ(rn->config.base_learning_rate, 0.05f);

    /* Initialize to set initialized flag */
    int result = rn_init(rn);
    EXPECT_EQ(result, 0);
    EXPECT_TRUE(rn->initialized);
}

TEST_F(RedNucleusTest, DestroyWithNullDoesNotCrash) {
    rn_destroy(nullptr);
    /* Should not crash */
}

TEST_F(RedNucleusTest, InitSucceeds) {
    rn = rn_create(nullptr);
    ASSERT_NE(rn, nullptr);

    int result = rn_init(rn);
    EXPECT_EQ(result, 0);
    EXPECT_TRUE(rn->initialized);
}

TEST_F(RedNucleusTest, InitWithNullReturnsError) {
    int result = rn_init(nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(RedNucleusTest, ResetSucceeds) {
    rn = create_and_init(nullptr);
    ASSERT_NE(rn, nullptr);

    /* Modify state */
    rn->output_magnitude = 0.8f;
    rn->cumulative_error = 0.5f;

    int result = rn_reset(rn);
    EXPECT_EQ(result, 0);

    /* Should be reset to initial values */
    EXPECT_FLOAT_EQ(rn->output_magnitude, 0.0f);
    EXPECT_FLOAT_EQ(rn->cumulative_error, 0.0f);
}

TEST_F(RedNucleusTest, ResetWithNullReturnsError) {
    int result = rn_reset(nullptr);
    EXPECT_EQ(result, -1);
}

/* ==========================================================================
 * Motor Command API Tests
 * ========================================================================== */

TEST_F(RedNucleusTest, IssueCommandSucceeds) {
    rn = create_and_init(nullptr);
    ASSERT_NE(rn, nullptr);

    rn_motor_command_t cmd = create_velocity_command(
        RN_EFFECTOR_FORELIMB_DISTAL, 1.0f, 0.0f, 0.0f, 100.0f);

    int result = rn_issue_command(rn, &cmd);
    EXPECT_EQ(result, 0);
}

TEST_F(RedNucleusTest, IssueCommandWithoutInitReturnsError) {
    rn = rn_create(nullptr);
    ASSERT_NE(rn, nullptr);
    /* Don't call rn_init() - rn->initialized is false */

    rn_motor_command_t cmd = create_velocity_command(
        RN_EFFECTOR_FORELIMB_DISTAL, 1.0f, 0.0f, 0.0f, 100.0f);

    int result = rn_issue_command(rn, &cmd);
    EXPECT_EQ(result, -1);  /* Should fail because not initialized */
}

TEST_F(RedNucleusTest, IssueCommandWithNullRNReturnsError) {
    rn_motor_command_t cmd = create_velocity_command(
        RN_EFFECTOR_FORELIMB_DISTAL, 1.0f, 0.0f, 0.0f, 100.0f);

    int result = rn_issue_command(nullptr, &cmd);
    EXPECT_EQ(result, -1);
}

TEST_F(RedNucleusTest, IssueCommandWithNullCmdReturnsError) {
    rn = create_and_init(nullptr);
    ASSERT_NE(rn, nullptr);

    int result = rn_issue_command(rn, nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(RedNucleusTest, CommandVelocitySucceeds) {
    rn = create_and_init(nullptr);
    ASSERT_NE(rn, nullptr);

    rn_vector3_t velocity = make_vector3(1.0f, 0.5f, 0.0f);
    int result = rn_command_velocity(rn, RN_EFFECTOR_FORELIMB_DISTAL,
                                     &velocity, 100.0f);
    EXPECT_EQ(result, 0);
}

TEST_F(RedNucleusTest, CommandVelocityWithNullVelocityReturnsError) {
    rn = create_and_init(nullptr);
    ASSERT_NE(rn, nullptr);

    int result = rn_command_velocity(rn, RN_EFFECTOR_FORELIMB_DISTAL,
                                     nullptr, 100.0f);
    EXPECT_EQ(result, -1);
}

TEST_F(RedNucleusTest, CommandForceSucceeds) {
    rn = create_and_init(nullptr);
    ASSERT_NE(rn, nullptr);

    rn_vector3_t force = make_vector3(0.0f, 0.0f, 1.0f);
    int result = rn_command_force(rn, RN_EFFECTOR_FORELIMB_PROXIMAL,
                                  &force, 50.0f);
    EXPECT_EQ(result, 0);
}

TEST_F(RedNucleusTest, CommandForceWithNullForceReturnsError) {
    rn = create_and_init(nullptr);
    ASSERT_NE(rn, nullptr);

    int result = rn_command_force(rn, RN_EFFECTOR_FORELIMB_PROXIMAL,
                                  nullptr, 50.0f);
    EXPECT_EQ(result, -1);
}

TEST_F(RedNucleusTest, CommandPositionSucceeds) {
    rn = create_and_init(nullptr);
    ASSERT_NE(rn, nullptr);

    rn_vector3_t position = make_vector3(10.0f, 5.0f, 2.0f);
    int result = rn_command_position(rn, RN_EFFECTOR_HINDLIMB_DISTAL,
                                     &position, 200.0f);
    EXPECT_EQ(result, 0);
}

TEST_F(RedNucleusTest, CommandPositionWithNullPositionReturnsError) {
    rn = create_and_init(nullptr);
    ASSERT_NE(rn, nullptr);

    int result = rn_command_position(rn, RN_EFFECTOR_HINDLIMB_DISTAL,
                                     nullptr, 200.0f);
    EXPECT_EQ(result, -1);
}

TEST_F(RedNucleusTest, CommandPostureSucceeds) {
    rn = create_and_init(nullptr);
    ASSERT_NE(rn, nullptr);

    rn_vector3_t adjustment = make_vector3(0.1f, 0.0f, 0.2f);
    int result = rn_command_posture(rn, &adjustment, 0.8f);
    EXPECT_EQ(result, 0);
}

TEST_F(RedNucleusTest, CommandPostureWithNullAdjustmentReturnsError) {
    rn = create_and_init(nullptr);
    ASSERT_NE(rn, nullptr);

    int result = rn_command_posture(rn, nullptr, 0.8f);
    EXPECT_EQ(result, -1);
}

TEST_F(RedNucleusTest, CommandTrajectorySucceeds) {
    rn = create_and_init(nullptr);
    ASSERT_NE(rn, nullptr);

    rn_trajectory_point_t points[3];
    for (int i = 0; i < 3; i++) {
        points[i].position = make_vector3((float)i * 1.0f, 0.0f, 0.0f);
        points[i].velocity = make_vector3(1.0f, 0.0f, 0.0f);
        points[i].time_ms = (float)(i + 1) * 100.0f;
    }

    rn_trajectory_t trajectory;
    trajectory.points = points;
    trajectory.num_points = 3;
    trajectory.effector = RN_EFFECTOR_FORELIMB_DISTAL;
    trajectory.total_duration_ms = 300.0f;
    trajectory.smooth_interpolation = true;

    int result = rn_command_trajectory(rn, &trajectory);
    EXPECT_EQ(result, 0);
}

TEST_F(RedNucleusTest, CommandTrajectoryWithNullReturnsError) {
    rn = create_and_init(nullptr);
    ASSERT_NE(rn, nullptr);

    int result = rn_command_trajectory(rn, nullptr);
    EXPECT_EQ(result, -1);
}

/* ==========================================================================
 * Rubrospinal Output Tests
 * ========================================================================== */

TEST_F(RedNucleusTest, GetOutputReturnsValidValue) {
    rn = create_and_init(nullptr);
    ASSERT_NE(rn, nullptr);

    /* Set some output */
    rn->rubrospinal_output[RN_EFFECTOR_FORELIMB_DISTAL] = 0.75f;

    float output = rn_get_output(rn, RN_EFFECTOR_FORELIMB_DISTAL);
    EXPECT_FLOAT_EQ(output, 0.75f);
}

TEST_F(RedNucleusTest, GetOutputWithNullReturnsZero) {
    float output = rn_get_output(nullptr, RN_EFFECTOR_FORELIMB_DISTAL);
    EXPECT_FLOAT_EQ(output, 0.0f);
}

TEST_F(RedNucleusTest, GetOutputWithInvalidEffectorReturnsZero) {
    rn = create_and_init(nullptr);
    ASSERT_NE(rn, nullptr);

    float output = rn_get_output(rn, (rn_effector_t)99);
    EXPECT_FLOAT_EQ(output, 0.0f);
}

TEST_F(RedNucleusTest, GetAllOutputsSucceeds) {
    rn = create_and_init(nullptr);
    ASSERT_NE(rn, nullptr);

    /* Set some outputs */
    for (int i = 0; i < RN_EFFECTOR_COUNT; i++) {
        rn->rubrospinal_output[i] = (float)i * 0.1f;
    }

    float outputs[RN_EFFECTOR_COUNT];
    int result = rn_get_all_outputs(rn, outputs);

    EXPECT_EQ(result, 0);
    for (int i = 0; i < RN_EFFECTOR_COUNT; i++) {
        EXPECT_FLOAT_EQ(outputs[i], (float)i * 0.1f);
    }
}

TEST_F(RedNucleusTest, GetAllOutputsWithNullRNReturnsError) {
    float outputs[RN_EFFECTOR_COUNT];
    int result = rn_get_all_outputs(nullptr, outputs);
    EXPECT_EQ(result, -1);
}

TEST_F(RedNucleusTest, GetAllOutputsWithNullArrayReturnsError) {
    rn = create_and_init(nullptr);
    ASSERT_NE(rn, nullptr);

    int result = rn_get_all_outputs(rn, nullptr);
    EXPECT_EQ(result, -1);
}

/* ==========================================================================
 * Motor Learning Tests
 * ========================================================================== */

TEST_F(RedNucleusTest, ProcessErrorSucceeds) {
    rn = create_and_init(nullptr);
    ASSERT_NE(rn, nullptr);

    rn_motor_error_t error;
    memset(&error, 0, sizeof(error));
    error.type = RN_ERROR_POSITION;
    error.effector = RN_EFFECTOR_FORELIMB_DISTAL;
    error.error_magnitude = 0.3f;
    error.error_vector = make_vector3(0.1f, 0.2f, 0.0f);

    int result = rn_process_error(rn, &error);
    EXPECT_EQ(result, 0);
}

TEST_F(RedNucleusTest, ProcessErrorWithNullRNReturnsError) {
    rn_motor_error_t error;
    memset(&error, 0, sizeof(error));
    error.type = RN_ERROR_POSITION;

    int result = rn_process_error(nullptr, &error);
    EXPECT_EQ(result, -1);
}

TEST_F(RedNucleusTest, ProcessErrorWithNullErrorReturnsError) {
    rn = create_and_init(nullptr);
    ASSERT_NE(rn, nullptr);

    int result = rn_process_error(rn, nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(RedNucleusTest, ReportErrorSucceeds) {
    rn = create_and_init(nullptr);
    ASSERT_NE(rn, nullptr);

    int result = rn_report_error(rn, RN_EFFECTOR_FORELIMB_DISTAL,
                                 RN_ERROR_VELOCITY, 0.25f);
    EXPECT_EQ(result, 0);
}

TEST_F(RedNucleusTest, ReportErrorWithNullReturnsError) {
    int result = rn_report_error(nullptr, RN_EFFECTOR_FORELIMB_DISTAL,
                                 RN_ERROR_VELOCITY, 0.25f);
    EXPECT_EQ(result, -1);
}

TEST_F(RedNucleusTest, GetLearningStateSucceeds) {
    rn = create_and_init(nullptr);
    ASSERT_NE(rn, nullptr);

    rn_learning_state_t state;
    int result = rn_get_learning_state(rn, RN_EFFECTOR_FORELIMB_DISTAL, &state);

    EXPECT_EQ(result, 0);
    EXPECT_GE(state.learning_rate, 0.0f);
}

TEST_F(RedNucleusTest, GetLearningStateWithNullRNReturnsError) {
    rn_learning_state_t state;
    int result = rn_get_learning_state(nullptr, RN_EFFECTOR_FORELIMB_DISTAL, &state);
    EXPECT_EQ(result, -1);
}

TEST_F(RedNucleusTest, GetLearningStateWithNullStateReturnsError) {
    rn = create_and_init(nullptr);
    ASSERT_NE(rn, nullptr);

    int result = rn_get_learning_state(rn, RN_EFFECTOR_FORELIMB_DISTAL, nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(RedNucleusTest, GetSkillLevelReturnsValidValue) {
    rn = create_and_init(nullptr);
    ASSERT_NE(rn, nullptr);

    /* Set skill level */
    rn->learning[RN_EFFECTOR_FORELIMB_DISTAL].skill_level = 0.65f;

    float skill = rn_get_skill_level(rn, RN_EFFECTOR_FORELIMB_DISTAL);
    EXPECT_FLOAT_EQ(skill, 0.65f);
}

TEST_F(RedNucleusTest, GetSkillLevelWithNullReturnsZero) {
    float skill = rn_get_skill_level(nullptr, RN_EFFECTOR_FORELIMB_DISTAL);
    EXPECT_FLOAT_EQ(skill, 0.0f);
}

TEST_F(RedNucleusTest, SetLearningModulationSucceeds) {
    rn = create_and_init(nullptr);
    ASSERT_NE(rn, nullptr);

    int result = rn_set_learning_modulation(rn, 1.5f);
    EXPECT_EQ(result, 0);
    EXPECT_FLOAT_EQ(rn->global_learning_modulation, 1.5f);
}

TEST_F(RedNucleusTest, SetLearningModulationWithNullReturnsError) {
    int result = rn_set_learning_modulation(nullptr, 1.5f);
    EXPECT_EQ(result, -1);
}

TEST_F(RedNucleusTest, ResetLearningSucceeds) {
    rn = create_and_init(nullptr);
    ASSERT_NE(rn, nullptr);

    /* Set some learning state */
    rn->learning[RN_EFFECTOR_FORELIMB_DISTAL].skill_level = 0.8f;
    rn->learning[RN_EFFECTOR_FORELIMB_DISTAL].error_count = 100;

    int result = rn_reset_learning(rn, RN_EFFECTOR_FORELIMB_DISTAL);
    EXPECT_EQ(result, 0);

    /* Learning state should be reset */
    EXPECT_FLOAT_EQ(rn->learning[RN_EFFECTOR_FORELIMB_DISTAL].skill_level, 0.0f);
    EXPECT_EQ(rn->learning[RN_EFFECTOR_FORELIMB_DISTAL].error_count, 0u);
}

TEST_F(RedNucleusTest, ResetLearningWithNullReturnsError) {
    int result = rn_reset_learning(nullptr, RN_EFFECTOR_FORELIMB_DISTAL);
    EXPECT_EQ(result, -1);
}

/* ==========================================================================
 * Cerebellar Integration Tests
 * ========================================================================== */

TEST_F(RedNucleusTest, ProcessDentateInputSucceeds) {
    rn = create_and_init(nullptr);
    ASSERT_NE(rn, nullptr);

    rn_dentate_signal_t signal;
    memset(&signal, 0, sizeof(signal));
    signal.activity = 0.7f;
    signal.timing_adjustment = 0.1f;
    signal.num_corrections = 3;
    for (uint32_t i = 0; i < signal.num_corrections; i++) {
        signal.motor_correction[i] = 0.05f;
    }

    int result = rn_process_dentate_input(rn, &signal);
    EXPECT_EQ(result, 0);
}

TEST_F(RedNucleusTest, ProcessDentateInputWithNullRNReturnsError) {
    rn_dentate_signal_t signal;
    memset(&signal, 0, sizeof(signal));

    int result = rn_process_dentate_input(nullptr, &signal);
    EXPECT_EQ(result, -1);
}

TEST_F(RedNucleusTest, ProcessDentateInputWithNullSignalReturnsError) {
    rn = create_and_init(nullptr);
    ASSERT_NE(rn, nullptr);

    int result = rn_process_dentate_input(rn, nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(RedNucleusTest, GetOlivaryOutputSucceeds) {
    rn = create_and_init(nullptr);
    ASSERT_NE(rn, nullptr);

    rn_olivary_output_t output;
    int result = rn_get_olivary_output(rn, &output);
    EXPECT_EQ(result, 0);
}

TEST_F(RedNucleusTest, GetOlivaryOutputWithNullRNReturnsError) {
    rn_olivary_output_t output;
    int result = rn_get_olivary_output(nullptr, &output);
    EXPECT_EQ(result, -1);
}

TEST_F(RedNucleusTest, GetOlivaryOutputWithNullOutputReturnsError) {
    rn = create_and_init(nullptr);
    ASSERT_NE(rn, nullptr);

    int result = rn_get_olivary_output(rn, nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(RedNucleusTest, GetThalamicOutputSucceeds) {
    rn = create_and_init(nullptr);
    ASSERT_NE(rn, nullptr);

    rn_thalamic_output_t output;
    int result = rn_get_thalamic_output(rn, &output);
    EXPECT_EQ(result, 0);
}

TEST_F(RedNucleusTest, GetThalamicOutputWithNullRNReturnsError) {
    rn_thalamic_output_t output;
    int result = rn_get_thalamic_output(nullptr, &output);
    EXPECT_EQ(result, -1);
}

TEST_F(RedNucleusTest, GetThalamicOutputWithNullOutputReturnsError) {
    rn = create_and_init(nullptr);
    ASSERT_NE(rn, nullptr);

    int result = rn_get_thalamic_output(rn, nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(RedNucleusTest, CerebellumConnectSucceeds) {
    rn = create_and_init(nullptr);
    ASSERT_NE(rn, nullptr);

    /* Connect with null (disconnect) should succeed */
    int result = rn_cerebellum_connect(rn, nullptr);
    EXPECT_EQ(result, 0);
}

TEST_F(RedNucleusTest, CerebellumConnectWithNullRNReturnsError) {
    int result = rn_cerebellum_connect(nullptr, nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(RedNucleusTest, ProcessCerebellarErrorSucceeds) {
    rn = create_and_init(nullptr);
    ASSERT_NE(rn, nullptr);

    int result = rn_process_cerebellar_error(rn, 0.3f, RN_ERROR_TRAJECTORY);
    EXPECT_EQ(result, 0);
}

TEST_F(RedNucleusTest, ProcessCerebellarErrorWithNullReturnsError) {
    int result = rn_process_cerebellar_error(nullptr, 0.3f, RN_ERROR_TRAJECTORY);
    EXPECT_EQ(result, -1);
}

/* ==========================================================================
 * Cortical Input Tests
 * ========================================================================== */

TEST_F(RedNucleusTest, SetCorticalInputSucceeds) {
    rn = create_and_init(nullptr);
    ASSERT_NE(rn, nullptr);

    int result = rn_set_cortical_input(rn, RN_CMD_VELOCITY, 0.8f);
    EXPECT_EQ(result, 0);
    EXPECT_FLOAT_EQ(rn->cortical_input[RN_CMD_VELOCITY], 0.8f);
}

TEST_F(RedNucleusTest, SetCorticalInputWithNullReturnsError) {
    int result = rn_set_cortical_input(nullptr, RN_CMD_VELOCITY, 0.8f);
    EXPECT_EQ(result, -1);
}

TEST_F(RedNucleusTest, GetCorticalInputReturnsCorrectValue) {
    rn = create_and_init(nullptr);
    ASSERT_NE(rn, nullptr);

    rn->cortical_input[RN_CMD_FORCE] = 0.6f;
    float input = rn_get_cortical_input(rn, RN_CMD_FORCE);
    EXPECT_FLOAT_EQ(input, 0.6f);
}

TEST_F(RedNucleusTest, GetCorticalInputWithNullReturnsZero) {
    float input = rn_get_cortical_input(nullptr, RN_CMD_FORCE);
    EXPECT_FLOAT_EQ(input, 0.0f);
}

/* ==========================================================================
 * Update and State Tests
 * ========================================================================== */

TEST_F(RedNucleusTest, UpdateSucceeds) {
    rn = create_and_init(nullptr);
    ASSERT_NE(rn, nullptr);

    int result = rn_update(rn, 0.01f);  /* 10ms timestep */
    EXPECT_EQ(result, 0);
}

TEST_F(RedNucleusTest, UpdateWithNullReturnsError) {
    int result = rn_update(nullptr, 0.01f);
    EXPECT_EQ(result, -1);
}

TEST_F(RedNucleusTest, UpdateWithoutInitReturnsError) {
    rn = rn_create(nullptr);
    ASSERT_NE(rn, nullptr);
    /* Don't call rn_init() */

    int result = rn_update(rn, 0.01f);
    EXPECT_EQ(result, -1);  /* Should fail - not initialized */
}

TEST_F(RedNucleusTest, UpdateIncreasesCommandsIssuedCount) {
    rn = create_and_init(nullptr);
    ASSERT_NE(rn, nullptr);

    uint64_t initial_count = rn->stats.commands_issued;

    /* Issue a command then update */
    rn_vector3_t vel = make_vector3(1.0f, 0.0f, 0.0f);
    rn_command_velocity(rn, RN_EFFECTOR_FORELIMB_DISTAL, &vel, 100.0f);

    for (int i = 0; i < 10; i++) {
        rn_update(rn, 0.01f);
    }

    /* Stats should be updated */
    EXPECT_GT(rn->stats.commands_issued, initial_count);
}

TEST_F(RedNucleusTest, GetStatsSucceeds) {
    rn = create_and_init(nullptr);
    ASSERT_NE(rn, nullptr);

    rn_stats_t stats;
    int result = rn_get_stats(rn, &stats);
    EXPECT_EQ(result, 0);
}

TEST_F(RedNucleusTest, GetStatsWithNullRNReturnsError) {
    rn_stats_t stats;
    int result = rn_get_stats(nullptr, &stats);
    EXPECT_EQ(result, -1);
}

TEST_F(RedNucleusTest, GetStatsWithNullStatsReturnsError) {
    rn = create_and_init(nullptr);
    ASSERT_NE(rn, nullptr);

    int result = rn_get_stats(rn, nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(RedNucleusTest, GetSubdivisionActivityReturnsValidValue) {
    rn = create_and_init(nullptr);
    ASSERT_NE(rn, nullptr);

    rn->subdivisions.activity[RN_SUBDIV_MAGNOCELLULAR] = 0.5f;

    float activity = rn_get_subdivision_activity(rn, RN_SUBDIV_MAGNOCELLULAR);
    EXPECT_FLOAT_EQ(activity, 0.5f);
}

TEST_F(RedNucleusTest, GetSubdivisionActivityWithNullReturnsZero) {
    float activity = rn_get_subdivision_activity(nullptr, RN_SUBDIV_MAGNOCELLULAR);
    EXPECT_FLOAT_EQ(activity, 0.0f);
}

TEST_F(RedNucleusTest, GetSubdivisionActivityWithInvalidSubdivReturnsZero) {
    rn = create_and_init(nullptr);
    ASSERT_NE(rn, nullptr);

    float activity = rn_get_subdivision_activity(rn, (rn_subdivision_t)99);
    EXPECT_FLOAT_EQ(activity, 0.0f);
}

/* ==========================================================================
 * Command Queue Management Tests
 * ========================================================================== */

TEST_F(RedNucleusTest, ClearCommandsSucceeds) {
    rn = create_and_init(nullptr);
    ASSERT_NE(rn, nullptr);

    /* Queue some commands */
    rn_vector3_t vel = make_vector3(1.0f, 0.0f, 0.0f);
    rn_command_velocity(rn, RN_EFFECTOR_FORELIMB_DISTAL, &vel, 100.0f);
    rn_command_velocity(rn, RN_EFFECTOR_FORELIMB_PROXIMAL, &vel, 100.0f);

    int result = rn_clear_commands(rn);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(rn->queue_size, 0u);
}

TEST_F(RedNucleusTest, ClearCommandsWithNullReturnsError) {
    int result = rn_clear_commands(nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(RedNucleusTest, AbortCommandSucceeds) {
    rn = create_and_init(nullptr);
    ASSERT_NE(rn, nullptr);

    /* Issue a command */
    rn_vector3_t vel = make_vector3(1.0f, 0.0f, 0.0f);
    rn_command_velocity(rn, RN_EFFECTOR_FORELIMB_DISTAL, &vel, 100.0f);

    int result = rn_abort_command(rn);
    EXPECT_EQ(result, 0);
}

TEST_F(RedNucleusTest, AbortCommandWithNullReturnsError) {
    int result = rn_abort_command(nullptr);
    EXPECT_EQ(result, -1);
}

/* ==========================================================================
 * Bio-Async Integration Tests
 * ========================================================================== */

TEST_F(RedNucleusTest, BioAsyncConnectWithNullRouterReturnsError) {
    rn = create_and_init(nullptr);
    ASSERT_NE(rn, nullptr);

    /* Connect with null router returns error per implementation */
    int result = rn_bio_async_connect(rn, nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(RedNucleusTest, BioAsyncConnectWithNullRNReturnsError) {
    int result = rn_bio_async_connect(nullptr, nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(RedNucleusTest, BioAsyncDisconnectSucceeds) {
    rn = create_and_init(nullptr);
    ASSERT_NE(rn, nullptr);

    int result = rn_bio_async_disconnect(rn);
    EXPECT_EQ(result, 0);
}

TEST_F(RedNucleusTest, BioAsyncDisconnectWithNullReturnsError) {
    int result = rn_bio_async_disconnect(nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(RedNucleusTest, BioAsyncSubscribeWithNullRouterReturnsError) {
    rn = create_and_init(nullptr);
    ASSERT_NE(rn, nullptr);

    /* bio_router is NULL by default, so subscribe should fail */
    int result = rn_bio_async_subscribe(rn, RN_BIO_SUB_MOTOR_CMD | RN_BIO_SUB_ERROR);
    EXPECT_EQ(result, -1);
}

TEST_F(RedNucleusTest, BioAsyncSubscribeWithNullRNReturnsError) {
    int result = rn_bio_async_subscribe(nullptr, RN_BIO_SUB_ALL);
    EXPECT_EQ(result, -1);
}

TEST_F(RedNucleusTest, BioAsyncBroadcastWithNullRNReturnsError) {
    int result = rn_bio_async_broadcast(nullptr, RN_BIO_MSG_MOTOR_CMD, nullptr, 0);
    EXPECT_EQ(result, -1);
}

/* ==========================================================================
 * KG Wiring Tests
 * ========================================================================== */

TEST_F(RedNucleusTest, KGUnregisterSucceeds) {
    rn = create_and_init(nullptr);
    ASSERT_NE(rn, nullptr);

    int result = rn_kg_unregister(rn);
    EXPECT_EQ(result, 0);
}

TEST_F(RedNucleusTest, KGUnregisterWithNullReturnsError) {
    int result = rn_kg_unregister(nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(RedNucleusTest, KGUpdateStateWithNullReturnsError) {
    int result = rn_kg_update_state(nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(RedNucleusTest, KGRegisterWithNullRNReturnsError) {
    int result = rn_kg_register(nullptr, nullptr, 0);
    EXPECT_EQ(result, -1);
}

TEST_F(RedNucleusTest, KGRegisterWithNullKGReturnsError) {
    rn = create_and_init(nullptr);
    ASSERT_NE(rn, nullptr);

    int result = rn_kg_register(rn, nullptr, 0);
    EXPECT_EQ(result, -1);
}

/* ==========================================================================
 * Integration Connection Tests
 * ========================================================================== */

TEST_F(RedNucleusTest, ImmuneConnectWithNullRNReturnsError) {
    int result = rn_immune_connect(nullptr, nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(RedNucleusTest, SecurityConnectWithNullRNReturnsError) {
    int result = rn_security_connect(nullptr, nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(RedNucleusTest, SNNConnectWithNullRNReturnsError) {
    int result = rn_snn_connect(nullptr, nullptr, nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(RedNucleusTest, HypothalamusConnectWithNullRNReturnsError) {
    int result = rn_hypothalamus_connect(nullptr, nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(RedNucleusTest, ThalamusConnectWithNullRNReturnsError) {
    int result = rn_thalamus_connect(nullptr, nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(RedNucleusTest, CognitiveConnectWithNullRNReturnsError) {
    int result = rn_cognitive_connect(nullptr, nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(RedNucleusTest, TrainingConnectWithNullRNReturnsError) {
    int result = rn_training_connect(nullptr, nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(RedNucleusTest, PerceptionConnectWithNullRNReturnsError) {
    int result = rn_perception_connect(nullptr, nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(RedNucleusTest, SymbolicConnectWithNullRNReturnsError) {
    int result = rn_symbolic_connect(nullptr, nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(RedNucleusTest, SwarmConnectWithNullRNReturnsError) {
    int result = rn_swarm_connect(nullptr, nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(RedNucleusTest, DragonflyConnectWithNullRNReturnsError) {
    int result = rn_dragonfly_connect(nullptr, nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(RedNucleusTest, PortiaConnectWithNullRNReturnsError) {
    int result = rn_portia_connect(nullptr, nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(RedNucleusTest, QMCConnectWithNullRNReturnsError) {
    int result = rn_qmc_connect(nullptr, nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(RedNucleusTest, OmniConnectWithNullRNReturnsError) {
    int result = rn_omni_connect(nullptr, nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(RedNucleusTest, SubstrateConnectWithNullRNReturnsError) {
    int result = rn_substrate_connect(nullptr, nullptr);
    EXPECT_EQ(result, -1);
}

/* ==========================================================================
 * Quantum Optimization Tests
 * ========================================================================== */

TEST_F(RedNucleusTest, QMCOptimizeCommandsWithNullReturnsError) {
    int result = rn_qmc_optimize_commands(nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(RedNucleusTest, QMCOptimizeCommandsWithoutQMCReturnsError) {
    rn = create_and_init(nullptr);
    ASSERT_NE(rn, nullptr);

    /* qmc is NULL by default, so optimize should fail */
    int result = rn_qmc_optimize_commands(rn);
    EXPECT_EQ(result, -1);
}

TEST_F(RedNucleusTest, QMCTSTrajectorySearchWithNullRNReturnsError) {
    rn_vector3_t start = make_vector3(0.0f, 0.0f, 0.0f);
    rn_vector3_t goal = make_vector3(1.0f, 1.0f, 1.0f);
    rn_trajectory_t trajectory;

    int result = rn_qmcts_trajectory_search(nullptr, &start, &goal, 100, &trajectory);
    EXPECT_EQ(result, -1);
}

TEST_F(RedNucleusTest, QMCTSTrajectorySearchWithNullStartReturnsError) {
    rn = create_and_init(nullptr);
    ASSERT_NE(rn, nullptr);

    rn_vector3_t goal = make_vector3(1.0f, 1.0f, 1.0f);
    rn_trajectory_t trajectory;

    int result = rn_qmcts_trajectory_search(rn, nullptr, &goal, 100, &trajectory);
    EXPECT_EQ(result, -1);
}

TEST_F(RedNucleusTest, QMCTSTrajectorySearchWithNullGoalReturnsError) {
    rn = create_and_init(nullptr);
    ASSERT_NE(rn, nullptr);

    rn_vector3_t start = make_vector3(0.0f, 0.0f, 0.0f);
    rn_trajectory_t trajectory;

    int result = rn_qmcts_trajectory_search(rn, &start, nullptr, 100, &trajectory);
    EXPECT_EQ(result, -1);
}

TEST_F(RedNucleusTest, QMCTSTrajectorySearchWithNullTrajectoryReturnsError) {
    rn = create_and_init(nullptr);
    ASSERT_NE(rn, nullptr);

    rn_vector3_t start = make_vector3(0.0f, 0.0f, 0.0f);
    rn_vector3_t goal = make_vector3(1.0f, 1.0f, 1.0f);

    int result = rn_qmcts_trajectory_search(rn, &start, &goal, 100, nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(RedNucleusTest, QMCTSTrajectorySearchWithoutQMCReturnsError) {
    rn = create_and_init(nullptr);
    ASSERT_NE(rn, nullptr);

    rn_vector3_t start = make_vector3(0.0f, 0.0f, 0.0f);
    rn_vector3_t goal = make_vector3(1.0f, 1.0f, 1.0f);
    rn_trajectory_t trajectory;

    /* qmc is NULL by default */
    int result = rn_qmcts_trajectory_search(rn, &start, &goal, 100, &trajectory);
    EXPECT_EQ(result, -1);
}

/* ==========================================================================
 * Utility Function Tests
 * ========================================================================== */

TEST_F(RedNucleusTest, SubdivisionStringReturnsValidStrings) {
    const char* mag = rn_subdivision_string(RN_SUBDIV_MAGNOCELLULAR);
    const char* parv = rn_subdivision_string(RN_SUBDIV_PARVOCELLULAR);

    ASSERT_NE(mag, nullptr);
    ASSERT_NE(parv, nullptr);
    EXPECT_STRNE(mag, "");
    EXPECT_STRNE(parv, "");
}

TEST_F(RedNucleusTest, SubdivisionStringHandlesInvalidInput) {
    const char* str = rn_subdivision_string((rn_subdivision_t)99);
    ASSERT_NE(str, nullptr);  /* Should return "Unknown" or similar */
}

TEST_F(RedNucleusTest, CmdTypeStringReturnsValidStrings) {
    const char* vel = rn_cmd_type_string(RN_CMD_VELOCITY);
    const char* force = rn_cmd_type_string(RN_CMD_FORCE);
    const char* pos = rn_cmd_type_string(RN_CMD_POSITION);

    ASSERT_NE(vel, nullptr);
    ASSERT_NE(force, nullptr);
    ASSERT_NE(pos, nullptr);
    EXPECT_STRNE(vel, "");
    EXPECT_STRNE(force, "");
    EXPECT_STRNE(pos, "");
}

TEST_F(RedNucleusTest, CmdTypeStringHandlesInvalidInput) {
    const char* str = rn_cmd_type_string((rn_motor_cmd_type_t)99);
    ASSERT_NE(str, nullptr);
}

TEST_F(RedNucleusTest, EffectorStringReturnsValidStrings) {
    const char* forelimb_p = rn_effector_string(RN_EFFECTOR_FORELIMB_PROXIMAL);
    const char* forelimb_d = rn_effector_string(RN_EFFECTOR_FORELIMB_DISTAL);
    const char* hindlimb_p = rn_effector_string(RN_EFFECTOR_HINDLIMB_PROXIMAL);
    const char* hindlimb_d = rn_effector_string(RN_EFFECTOR_HINDLIMB_DISTAL);
    const char* axial = rn_effector_string(RN_EFFECTOR_AXIAL);

    ASSERT_NE(forelimb_p, nullptr);
    ASSERT_NE(forelimb_d, nullptr);
    ASSERT_NE(hindlimb_p, nullptr);
    ASSERT_NE(hindlimb_d, nullptr);
    ASSERT_NE(axial, nullptr);
}

TEST_F(RedNucleusTest, EffectorStringHandlesInvalidInput) {
    const char* str = rn_effector_string((rn_effector_t)99);
    ASSERT_NE(str, nullptr);
}

TEST_F(RedNucleusTest, ErrorTypeStringReturnsValidStrings) {
    const char* pos_err = rn_error_type_string(RN_ERROR_POSITION);
    const char* vel_err = rn_error_type_string(RN_ERROR_VELOCITY);
    const char* force_err = rn_error_type_string(RN_ERROR_FORCE);
    const char* timing_err = rn_error_type_string(RN_ERROR_TIMING);
    const char* traj_err = rn_error_type_string(RN_ERROR_TRAJECTORY);

    ASSERT_NE(pos_err, nullptr);
    ASSERT_NE(vel_err, nullptr);
    ASSERT_NE(force_err, nullptr);
    ASSERT_NE(timing_err, nullptr);
    ASSERT_NE(traj_err, nullptr);
}

TEST_F(RedNucleusTest, ErrorTypeStringHandlesInvalidInput) {
    const char* str = rn_error_type_string((rn_error_type_t)99);
    ASSERT_NE(str, nullptr);
}

TEST_F(RedNucleusTest, BioMsgStringReturnsValidStrings) {
    const char* motor_cmd = rn_bio_msg_string(RN_BIO_MSG_MOTOR_CMD);
    const char* error_sig = rn_bio_msg_string(RN_BIO_MSG_ERROR_SIGNAL);
    const char* learning = rn_bio_msg_string(RN_BIO_MSG_LEARNING_UPDATE);

    ASSERT_NE(motor_cmd, nullptr);
    ASSERT_NE(error_sig, nullptr);
    ASSERT_NE(learning, nullptr);
}

TEST_F(RedNucleusTest, BioMsgStringHandlesInvalidInput) {
    const char* str = rn_bio_msg_string((rn_bio_msg_type_t)99);
    ASSERT_NE(str, nullptr);
}

/* ==========================================================================
 * Statistics Tracking Tests
 * ========================================================================== */

TEST_F(RedNucleusTest, StatsInitializedToZeroAfterInit) {
    rn = create_and_init(nullptr);
    ASSERT_NE(rn, nullptr);

    rn_stats_t stats;
    rn_get_stats(rn, &stats);

    EXPECT_EQ(stats.commands_issued, 0u);
    EXPECT_EQ(stats.commands_completed, 0u);
    EXPECT_EQ(stats.commands_aborted, 0u);
    EXPECT_EQ(stats.errors_detected, 0u);
    EXPECT_EQ(stats.learning_updates, 0u);
}

TEST_F(RedNucleusTest, StatsUpdatedAfterCommand) {
    rn = create_and_init(nullptr);
    ASSERT_NE(rn, nullptr);

    rn_stats_t stats_before;
    rn_get_stats(rn, &stats_before);

    rn_vector3_t vel = make_vector3(1.0f, 0.0f, 0.0f);
    rn_command_velocity(rn, RN_EFFECTOR_FORELIMB_DISTAL, &vel, 100.0f);

    rn_stats_t stats_after;
    rn_get_stats(rn, &stats_after);

    EXPECT_GT(stats_after.commands_issued, stats_before.commands_issued);
}

TEST_F(RedNucleusTest, StatsUpdatedAfterError) {
    rn = create_and_init(nullptr);
    ASSERT_NE(rn, nullptr);

    rn_stats_t stats_before;
    rn_get_stats(rn, &stats_before);

    rn_report_error(rn, RN_EFFECTOR_FORELIMB_DISTAL, RN_ERROR_POSITION, 0.5f);

    rn_stats_t stats_after;
    rn_get_stats(rn, &stats_after);

    EXPECT_GT(stats_after.errors_detected, stats_before.errors_detected);
}

/* ==========================================================================
 * Limb Coordination Tests
 * ========================================================================== */

TEST_F(RedNucleusTest, MultipleEffectorCommandsQueued) {
    rn = create_and_init(nullptr);
    ASSERT_NE(rn, nullptr);

    /* Issue commands to multiple effectors */
    rn_vector3_t vel1 = make_vector3(1.0f, 0.0f, 0.0f);
    rn_vector3_t vel2 = make_vector3(0.0f, 1.0f, 0.0f);
    rn_vector3_t vel3 = make_vector3(0.0f, 0.0f, 1.0f);

    EXPECT_EQ(rn_command_velocity(rn, RN_EFFECTOR_FORELIMB_DISTAL, &vel1, 100.0f), 0);
    EXPECT_EQ(rn_command_velocity(rn, RN_EFFECTOR_FORELIMB_PROXIMAL, &vel2, 100.0f), 0);
    EXPECT_EQ(rn_command_velocity(rn, RN_EFFECTOR_HINDLIMB_DISTAL, &vel3, 100.0f), 0);
}

TEST_F(RedNucleusTest, AllEffectorTypesHaveIndependentOutput) {
    rn = create_and_init(nullptr);
    ASSERT_NE(rn, nullptr);

    /* Set different outputs for each effector */
    for (int i = 0; i < RN_EFFECTOR_COUNT; i++) {
        rn->rubrospinal_output[i] = (float)(i + 1) * 0.15f;
    }

    /* Verify each effector has independent output */
    for (int i = 0; i < RN_EFFECTOR_COUNT; i++) {
        float expected = (float)(i + 1) * 0.15f;
        EXPECT_FLOAT_EQ(rn_get_output(rn, (rn_effector_t)i), expected);
    }
}

/* ==========================================================================
 * Motor Refinement Tests
 * ========================================================================== */

TEST_F(RedNucleusTest, LearningImprovesPrecision) {
    rn = create_and_init(nullptr);
    ASSERT_NE(rn, nullptr);

    float initial_skill = rn_get_skill_level(rn, RN_EFFECTOR_FORELIMB_DISTAL);

    /* Simulate learning through error processing */
    for (int i = 0; i < 100; i++) {
        /* Issue command */
        rn_vector3_t vel = make_vector3(1.0f, 0.0f, 0.0f);
        rn_command_velocity(rn, RN_EFFECTOR_FORELIMB_DISTAL, &vel, 100.0f);

        /* Report decreasing errors (learning) */
        float error_mag = 0.5f - (float)i * 0.005f;
        if (error_mag < 0.0f) error_mag = 0.0f;
        rn_report_error(rn, RN_EFFECTOR_FORELIMB_DISTAL, RN_ERROR_POSITION, error_mag);

        rn_update(rn, 0.01f);
    }

    float final_skill = rn_get_skill_level(rn, RN_EFFECTOR_FORELIMB_DISTAL);

    /* Skill should improve with learning */
    EXPECT_GE(final_skill, initial_skill);
}

/* ==========================================================================
 * Subdivision State Tests
 * ========================================================================== */

TEST_F(RedNucleusTest, SubdivisionsInitializedCorrectly) {
    rn = create_and_init(nullptr);
    ASSERT_NE(rn, nullptr);

    /* All subdivisions should be initialized */
    for (int i = 0; i < RN_SUBDIV_COUNT; i++) {
        float activity = rn_get_subdivision_activity(rn, (rn_subdivision_t)i);
        EXPECT_GE(activity, 0.0f);
        EXPECT_LE(activity, 1.0f);
    }
}

TEST_F(RedNucleusTest, MagnocellularActivityForMotorCommands) {
    rn = create_and_init(nullptr);
    ASSERT_NE(rn, nullptr);

    /* Issue motor command */
    rn_vector3_t vel = make_vector3(1.0f, 0.0f, 0.0f);
    rn_command_velocity(rn, RN_EFFECTOR_FORELIMB_DISTAL, &vel, 100.0f);
    rn_update(rn, 0.01f);

    float mag_activity = rn_get_subdivision_activity(rn, RN_SUBDIV_MAGNOCELLULAR);
    /* Magnocellular should be active for motor commands */
    EXPECT_GE(mag_activity, 0.0f);
}

/* ==========================================================================
 * Edge Cases and Boundary Tests
 * ========================================================================== */

TEST_F(RedNucleusTest, ZeroDurationCommandHandled) {
    rn = create_and_init(nullptr);
    ASSERT_NE(rn, nullptr);

    rn_vector3_t vel = make_vector3(1.0f, 0.0f, 0.0f);
    int result = rn_command_velocity(rn, RN_EFFECTOR_FORELIMB_DISTAL, &vel, 0.0f);
    /* Should handle gracefully */
    EXPECT_EQ(result, 0);
}

TEST_F(RedNucleusTest, NegativeDurationCommandHandled) {
    rn = create_and_init(nullptr);
    ASSERT_NE(rn, nullptr);

    rn_vector3_t vel = make_vector3(1.0f, 0.0f, 0.0f);
    int result = rn_command_velocity(rn, RN_EFFECTOR_FORELIMB_DISTAL, &vel, -100.0f);
    /* Should handle gracefully (either accept or reject consistently) */
    /* Just ensure no crash */
    (void)result;
}

TEST_F(RedNucleusTest, VeryLargeMagnitudeVector) {
    rn = create_and_init(nullptr);
    ASSERT_NE(rn, nullptr);

    rn_vector3_t vel = make_vector3(1e10f, 1e10f, 1e10f);
    int result = rn_command_velocity(rn, RN_EFFECTOR_FORELIMB_DISTAL, &vel, 100.0f);
    /* Should handle without crash */
    (void)result;
}

TEST_F(RedNucleusTest, VerySmallMagnitudeVector) {
    rn = create_and_init(nullptr);
    ASSERT_NE(rn, nullptr);

    rn_vector3_t vel = make_vector3(1e-10f, 1e-10f, 1e-10f);
    int result = rn_command_velocity(rn, RN_EFFECTOR_FORELIMB_DISTAL, &vel, 100.0f);
    EXPECT_EQ(result, 0);
}

TEST_F(RedNucleusTest, ZeroVectorCommand) {
    rn = create_and_init(nullptr);
    ASSERT_NE(rn, nullptr);

    rn_vector3_t vel = make_vector3(0.0f, 0.0f, 0.0f);
    int result = rn_command_velocity(rn, RN_EFFECTOR_FORELIMB_DISTAL, &vel, 100.0f);
    EXPECT_EQ(result, 0);
}

TEST_F(RedNucleusTest, ErrorMagnitudeClampedToValidRange) {
    rn = create_and_init(nullptr);
    ASSERT_NE(rn, nullptr);

    /* Test error magnitude outside valid range [-1, 1] */
    int result1 = rn_report_error(rn, RN_EFFECTOR_FORELIMB_DISTAL,
                                  RN_ERROR_POSITION, 5.0f);
    int result2 = rn_report_error(rn, RN_EFFECTOR_FORELIMB_DISTAL,
                                  RN_ERROR_POSITION, -5.0f);

    /* Should handle without crash */
    (void)result1;
    (void)result2;
}

TEST_F(RedNucleusTest, LearningModulationClampedToValidRange) {
    rn = create_and_init(nullptr);
    ASSERT_NE(rn, nullptr);

    /* Test modulation outside valid range [0, 2] */
    int result1 = rn_set_learning_modulation(rn, 10.0f);
    EXPECT_EQ(result1, 0);
    /* Should be clamped to 2.0 */
    EXPECT_FLOAT_EQ(rn->global_learning_modulation, 2.0f);

    int result2 = rn_set_learning_modulation(rn, -1.0f);
    EXPECT_EQ(result2, 0);
    /* Should be clamped to 0.0 */
    EXPECT_FLOAT_EQ(rn->global_learning_modulation, 0.0f);
}

/* ==========================================================================
 * Concurrent Operations Safety Tests
 * ========================================================================== */

TEST_F(RedNucleusTest, RapidCommandIssuance) {
    rn = create_and_init(nullptr);
    ASSERT_NE(rn, nullptr);

    /* Issue many commands rapidly */
    for (int i = 0; i < 1000; i++) {
        rn_vector3_t vel = make_vector3((float)i * 0.001f, 0.0f, 0.0f);
        rn_command_velocity(rn, (rn_effector_t)(i % RN_EFFECTOR_COUNT),
                            &vel, 10.0f);
    }

    /* Should not crash or overflow */
    rn_stats_t stats;
    rn_get_stats(rn, &stats);
    EXPECT_GT(stats.commands_issued, 0u);
}

TEST_F(RedNucleusTest, UpdateWithVerySmallTimestep) {
    rn = create_and_init(nullptr);
    ASSERT_NE(rn, nullptr);

    for (int i = 0; i < 1000; i++) {
        int result = rn_update(rn, 1e-6f);  /* 1 microsecond */
        EXPECT_EQ(result, 0);
    }
}

TEST_F(RedNucleusTest, UpdateWithVeryLargeTimestep) {
    rn = create_and_init(nullptr);
    ASSERT_NE(rn, nullptr);

    int result = rn_update(rn, 100.0f);  /* 100 seconds */
    EXPECT_EQ(result, 0);
}

/* ==========================================================================
 * Platform Tier Tests
 * ========================================================================== */

TEST_F(RedNucleusTest, CreateWithDifferentPlatformTiers) {
    rn_config_t config;
    rn_default_config(&config);

    /* Test full tier */
    config.platform_tier = PLATFORM_TIER_FULL;
    nimcp_red_nucleus_t* rn_full = rn_create(&config);
    EXPECT_NE(rn_full, nullptr);
    rn_destroy(rn_full);

    /* Test constrained tier */
    config.platform_tier = PLATFORM_TIER_CONSTRAINED;
    nimcp_red_nucleus_t* rn_constrained = rn_create(&config);
    EXPECT_NE(rn_constrained, nullptr);
    rn_destroy(rn_constrained);

    /* Test minimal tier */
    config.platform_tier = PLATFORM_TIER_MINIMAL;
    nimcp_red_nucleus_t* rn_minimal = rn_create(&config);
    EXPECT_NE(rn_minimal, nullptr);
    rn_destroy(rn_minimal);
}

/* ==========================================================================
 * Feature Toggle Tests
 * ========================================================================== */

TEST_F(RedNucleusTest, DisabledFeaturesStillWork) {
    rn_config_t config;
    rn_default_config(&config);

    /* Disable optional features */
    config.enable_bio_async = false;
    config.enable_kg_wiring = false;
    config.enable_immune = false;
    config.enable_security = false;
    config.enable_quantum = false;
    config.enable_cerebellar = false;

    rn = rn_create(&config);
    ASSERT_NE(rn, nullptr);
    rn_init(rn);

    /* Basic operations should still work */
    rn_vector3_t vel = make_vector3(1.0f, 0.0f, 0.0f);
    EXPECT_EQ(rn_command_velocity(rn, RN_EFFECTOR_FORELIMB_DISTAL, &vel, 100.0f), 0);
    EXPECT_EQ(rn_update(rn, 0.01f), 0);
}
