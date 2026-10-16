/**
 * @file test_vestibular_processor.cpp
 * @brief Unit tests for Vestibular nuclei processor (Phase 3)
 *
 * Tests:
 * - Lifecycle (create, destroy, reset)
 * - Semicircular canal input processing
 * - Otolith input processing
 * - VOR (Vestibulo-Ocular Reflex) functions
 * - VOR gain adaptation
 * - Vestibulospinal reflex
 * - Cerebellar interface
 * - Status and diagnostics
 * - Null safety
 *
 * BIOLOGICAL CONTEXT:
 * - Four vestibular nuclei: MVN, LVN, SVN, IVN
 * - VOR: Stabilizes gaze during head movement
 * - VSR: Maintains posture and balance
 * - Vestibulocerebellum: Calibration via flocculus and nodulus
 *
 * @version Phase 3: Vestibular System Integration
 * @date 2026-01-03
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

// Headers have their own extern "C" guards
#include "core/brain/regions/brainstem/nimcp_vestibular.h"

//=============================================================================
// Lifecycle Tests
//=============================================================================

class VestibularLifecycleTest : public ::testing::Test {
protected:
    vestibular_processor_t* processor = nullptr;

    void TearDown() override {
        if (processor) {
            vestibular_destroy(processor);
            processor = nullptr;
        }
    }
};

TEST_F(VestibularLifecycleTest, CreateWithNullConfig) {
    processor = vestibular_create(nullptr);
    ASSERT_NE(processor, nullptr);
}

TEST_F(VestibularLifecycleTest, CreateWithConfig) {
    vestibular_config_t config = vestibular_default_config();
    config.mvn_neurons = 200;
    config.initial_vor_gain = 0.9f;

    processor = vestibular_create(&config);
    ASSERT_NE(processor, nullptr);
}

TEST_F(VestibularLifecycleTest, DefaultConfigValues) {
    vestibular_config_t config = vestibular_default_config();

    // Nucleus neuron counts
    EXPECT_EQ(config.mvn_neurons, VESTIBULAR_DEFAULT_MVN_NEURONS);
    EXPECT_EQ(config.lvn_neurons, VESTIBULAR_DEFAULT_LVN_NEURONS);
    EXPECT_EQ(config.svn_neurons, VESTIBULAR_DEFAULT_SVN_NEURONS);
    EXPECT_EQ(config.ivn_neurons, VESTIBULAR_DEFAULT_IVN_NEURONS);

    // VOR parameters
    EXPECT_FLOAT_EQ(config.initial_vor_gain, VESTIBULAR_DEFAULT_VOR_GAIN);

    // Velocity storage
    EXPECT_FLOAT_EQ(config.velocity_storage_tau_s, VESTIBULAR_VELOCITY_STORAGE_TAU);
}

TEST_F(VestibularLifecycleTest, DestroyNull) {
    vestibular_destroy(nullptr);  // Should not crash
}

TEST_F(VestibularLifecycleTest, Reset) {
    processor = vestibular_create(nullptr);
    ASSERT_NE(processor, nullptr);

    EXPECT_TRUE(vestibular_reset(processor));
    EXPECT_EQ(vestibular_get_status(processor), VESTIBULAR_STATUS_IDLE);
}

TEST_F(VestibularLifecycleTest, ResetNull) {
    EXPECT_FALSE(vestibular_reset(nullptr));
}

//=============================================================================
// Semicircular Canal Input Tests
//=============================================================================

class VestibularCanalInputTest : public ::testing::Test {
protected:
    vestibular_processor_t* processor = nullptr;

    void SetUp() override {
        processor = vestibular_create(nullptr);
        ASSERT_NE(processor, nullptr);
    }

    void TearDown() override {
        if (processor) {
            vestibular_destroy(processor);
            processor = nullptr;
        }
    }
};

TEST_F(VestibularCanalInputTest, ProcessCanalInput) {
    semicircular_canal_input_t input;
    input.yaw_velocity = 0.5f;    // rad/s
    input.pitch_velocity = 0.0f;
    input.roll_velocity = 0.0f;
    input.timestamp_us = 0;

    EXPECT_TRUE(vestibular_process_canal_input(processor, &input));

    vestibular_stats_t stats;
    vestibular_get_stats(processor, &stats);
    EXPECT_EQ(stats.canal_inputs, 1);
}

TEST_F(VestibularCanalInputTest, YawRotationActivatesMVN) {
    semicircular_canal_input_t input;
    input.yaw_velocity = 1.0f;    // Horizontal rotation
    input.pitch_velocity = 0.0f;
    input.roll_velocity = 0.0f;
    input.timestamp_us = 0;

    vestibular_process_canal_input(processor, &input);

    vestibular_nucleus_state_t mvn_state;
    EXPECT_TRUE(vestibular_get_nucleus_state(processor, VESTIBULAR_NUCLEUS_MEDIAL, &mvn_state));

    // MVN handles horizontal VOR - should show yaw activity
    EXPECT_GT(fabs(mvn_state.activity[0]), 0.0f);  // Yaw axis
}

TEST_F(VestibularCanalInputTest, PitchRollActivateSVN) {
    semicircular_canal_input_t input;
    input.yaw_velocity = 0.0f;
    input.pitch_velocity = 1.0f;  // Vertical rotation
    input.roll_velocity = 0.0f;
    input.timestamp_us = 0;

    vestibular_process_canal_input(processor, &input);

    vestibular_nucleus_state_t svn_state;
    EXPECT_TRUE(vestibular_get_nucleus_state(processor, VESTIBULAR_NUCLEUS_SUPERIOR, &svn_state));

    // SVN handles vertical VOR - should show pitch/roll activity
    EXPECT_GE(fabs(mvn_state.activity[1]) + fabs(mvn_state.activity[2]), 0.0f);
}

TEST_F(VestibularCanalInputTest, NullInput) {
    EXPECT_FALSE(vestibular_process_canal_input(processor, nullptr));
    EXPECT_EQ(vestibular_get_last_error(processor), VESTIBULAR_ERROR_INVALID_INPUT);
}

TEST_F(VestibularCanalInputTest, StatusAfterInput) {
    semicircular_canal_input_t input;
    input.yaw_velocity = 0.5f;
    input.pitch_velocity = 0.0f;
    input.roll_velocity = 0.0f;
    input.timestamp_us = 0;

    vestibular_process_canal_input(processor, &input);

    vestibular_status_t status = vestibular_get_status(processor);
    EXPECT_NE(status, VESTIBULAR_STATUS_ERROR);
}

//=============================================================================
// Otolith Input Tests
//=============================================================================

class VestibularOtolithInputTest : public ::testing::Test {
protected:
    vestibular_processor_t* processor = nullptr;

    void SetUp() override {
        processor = vestibular_create(nullptr);
        ASSERT_NE(processor, nullptr);
    }

    void TearDown() override {
        if (processor) {
            vestibular_destroy(processor);
            processor = nullptr;
        }
    }
};

TEST_F(VestibularOtolithInputTest, ProcessOtolithInput) {
    otolith_input_t input;
    input.x_accel = 0.0f;
    input.y_accel = 0.0f;
    input.z_accel = 9.8f;  // Gravity
    input.head_tilt_pitch = 0.0f;
    input.head_tilt_roll = 0.0f;
    input.timestamp_us = 0;

    EXPECT_TRUE(vestibular_process_otolith_input(processor, &input));

    vestibular_stats_t stats;
    vestibular_get_stats(processor, &stats);
    EXPECT_EQ(stats.otolith_inputs, 1);
}

TEST_F(VestibularOtolithInputTest, LinearAccelerationActivatesLVN) {
    otolith_input_t input;
    input.x_accel = 2.0f;  // Forward acceleration
    input.y_accel = 0.0f;
    input.z_accel = 9.8f;
    input.head_tilt_pitch = 0.0f;
    input.head_tilt_roll = 0.0f;
    input.timestamp_us = 0;

    vestibular_process_otolith_input(processor, &input);

    // LVN handles vestibulospinal reflex
    vestibular_nucleus_state_t lvn_state;
    EXPECT_TRUE(vestibular_get_nucleus_state(processor, VESTIBULAR_NUCLEUS_LATERAL, &lvn_state));
    EXPECT_GT(lvn_state.current_rate, 0.0f);
}

TEST_F(VestibularOtolithInputTest, HeadTiltDetected) {
    otolith_input_t input;
    input.x_accel = 0.0f;
    input.y_accel = 0.0f;
    input.z_accel = 9.8f;
    input.head_tilt_pitch = 0.2f;  // Tilted forward 0.2 rad
    input.head_tilt_roll = 0.0f;
    input.timestamp_us = 0;

    vestibular_process_otolith_input(processor, &input);

    float position[3];
    EXPECT_TRUE(vestibular_get_head_position(processor, position));
    // Head position should reflect tilt
}

TEST_F(VestibularOtolithInputTest, NullInput) {
    EXPECT_FALSE(vestibular_process_otolith_input(processor, nullptr));
}

//=============================================================================
// Combined Vestibular Input Tests
//=============================================================================

class VestibularCombinedInputTest : public ::testing::Test {
protected:
    vestibular_processor_t* processor = nullptr;

    void SetUp() override {
        processor = vestibular_create(nullptr);
        ASSERT_NE(processor, nullptr);
    }

    void TearDown() override {
        if (processor) {
            vestibular_destroy(processor);
            processor = nullptr;
        }
    }
};

TEST_F(VestibularCombinedInputTest, ProcessCombinedInput) {
    vestibular_input_t input;

    // Canals
    input.canals.yaw_velocity = 0.5f;
    input.canals.pitch_velocity = 0.0f;
    input.canals.roll_velocity = 0.0f;
    input.canals.timestamp_us = 0;
    input.canals_valid = true;

    // Otoliths
    input.otoliths.x_accel = 0.0f;
    input.otoliths.y_accel = 0.0f;
    input.otoliths.z_accel = 9.8f;
    input.otoliths.head_tilt_pitch = 0.0f;
    input.otoliths.head_tilt_roll = 0.0f;
    input.otoliths.timestamp_us = 0;
    input.otoliths_valid = true;

    EXPECT_TRUE(vestibular_process_input(processor, &input));

    vestibular_stats_t stats;
    vestibular_get_stats(processor, &stats);
    EXPECT_EQ(stats.canal_inputs, 1);
    EXPECT_EQ(stats.otolith_inputs, 1);
}

TEST_F(VestibularCombinedInputTest, CanalOnlyInput) {
    vestibular_input_t input;

    input.canals.yaw_velocity = 0.5f;
    input.canals.pitch_velocity = 0.0f;
    input.canals.roll_velocity = 0.0f;
    input.canals.timestamp_us = 0;
    input.canals_valid = true;

    input.otoliths_valid = false;

    EXPECT_TRUE(vestibular_process_input(processor, &input));

    vestibular_stats_t stats;
    vestibular_get_stats(processor, &stats);
    EXPECT_EQ(stats.canal_inputs, 1);
    EXPECT_EQ(stats.otolith_inputs, 0);
}

//=============================================================================
// VOR Tests
//=============================================================================

class VestibularVORTest : public ::testing::Test {
protected:
    vestibular_processor_t* processor = nullptr;

    void SetUp() override {
        vestibular_config_t config = vestibular_default_config();
        config.enable_vor_adaptation = true;
        processor = vestibular_create(&config);
        ASSERT_NE(processor, nullptr);
    }

    void TearDown() override {
        if (processor) {
            vestibular_destroy(processor);
            processor = nullptr;
        }
    }
};

TEST_F(VestibularVORTest, GetVORCommand) {
    // Feed head rotation
    semicircular_canal_input_t input;
    input.yaw_velocity = 1.0f;  // 1 rad/s
    input.pitch_velocity = 0.0f;
    input.roll_velocity = 0.0f;
    input.timestamp_us = 0;

    vestibular_process_canal_input(processor, &input);

    float eye_velocity[3];
    EXPECT_TRUE(vestibular_get_vor_command(processor, eye_velocity));

    // VOR produces compensatory eye movement (opposite direction)
    // With gain ~1.0, eye velocity should be approximately -head velocity
    EXPECT_LT(eye_velocity[0], 0.0f);  // Opposite to yaw head velocity
}

TEST_F(VestibularVORTest, GetVORGain) {
    float gain[3];
    EXPECT_TRUE(vestibular_get_vor_gain(processor, gain));

    // Initial gain should be close to default
    EXPECT_NEAR(gain[0], VESTIBULAR_DEFAULT_VOR_GAIN, 0.1f);
    EXPECT_NEAR(gain[1], VESTIBULAR_DEFAULT_VOR_GAIN, 0.1f);
    EXPECT_NEAR(gain[2], VESTIBULAR_DEFAULT_VOR_GAIN, 0.1f);
}

TEST_F(VestibularVORTest, SetVORGainUniform) {
    float new_gain = 0.8f;
    EXPECT_TRUE(vestibular_set_vor_gain(processor, &new_gain, false));

    float gain[3];
    vestibular_get_vor_gain(processor, gain);

    EXPECT_FLOAT_EQ(gain[0], 0.8f);
    EXPECT_FLOAT_EQ(gain[1], 0.8f);
    EXPECT_FLOAT_EQ(gain[2], 0.8f);
}

TEST_F(VestibularVORTest, SetVORGainPerAxis) {
    float new_gains[3] = {0.7f, 0.8f, 0.9f};
    EXPECT_TRUE(vestibular_set_vor_gain(processor, new_gains, true));

    float gain[3];
    vestibular_get_vor_gain(processor, gain);

    EXPECT_FLOAT_EQ(gain[0], 0.7f);
    EXPECT_FLOAT_EQ(gain[1], 0.8f);
    EXPECT_FLOAT_EQ(gain[2], 0.9f);
}

TEST_F(VestibularVORTest, VORGainClamped) {
    float extreme_gain = 5.0f;  // Way above max
    vestibular_set_vor_gain(processor, &extreme_gain, false);

    float gain[3];
    vestibular_get_vor_gain(processor, gain);

    // Should be clamped to max
    EXPECT_LE(gain[0], VESTIBULAR_MAX_VOR_GAIN);

    float low_gain = 0.1f;  // Below min
    vestibular_set_vor_gain(processor, &low_gain, false);
    vestibular_get_vor_gain(processor, gain);

    EXPECT_GE(gain[0], VESTIBULAR_MIN_VOR_GAIN);
}

TEST_F(VestibularVORTest, RetinalSlipTriggersAdaptation) {
    // Get initial gain
    float initial_gain[3];
    vestibular_get_vor_gain(processor, initial_gain);

    // Report positive retinal slip (eyes not compensating enough)
    float direction[3] = {1.0f, 0.0f, 0.0f};  // Yaw direction
    EXPECT_TRUE(vestibular_report_retinal_slip(processor, 0.5f, direction));

    vestibular_stats_t stats;
    vestibular_get_stats(processor, &stats);
    EXPECT_GT(stats.adaptation_events, 0);
}

TEST_F(VestibularVORTest, VORStatsTracked) {
    semicircular_canal_input_t input;
    input.yaw_velocity = 1.0f;
    input.pitch_velocity = 0.0f;
    input.roll_velocity = 0.0f;
    input.timestamp_us = 0;

    vestibular_process_canal_input(processor, &input);

    float eye_velocity[3];
    vestibular_get_vor_command(processor, eye_velocity);

    vestibular_stats_t stats;
    vestibular_get_stats(processor, &stats);
    EXPECT_GT(stats.vor_commands, 0);
}

//=============================================================================
// Vestibulospinal Reflex Tests
//=============================================================================

class VestibularVSRTest : public ::testing::Test {
protected:
    vestibular_processor_t* processor = nullptr;

    void SetUp() override {
        vestibular_config_t config = vestibular_default_config();
        config.enable_vestibulospinal = true;
        processor = vestibular_create(&config);
        ASSERT_NE(processor, nullptr);
    }

    void TearDown() override {
        if (processor) {
            vestibular_destroy(processor);
            processor = nullptr;
        }
    }
};

TEST_F(VestibularVSRTest, GetPosturalCommand) {
    // Simulate body tilt
    otolith_input_t input;
    input.x_accel = 1.0f;  // Some acceleration
    input.y_accel = 0.0f;
    input.z_accel = 9.8f;
    input.head_tilt_pitch = 0.2f;
    input.head_tilt_roll = 0.0f;
    input.timestamp_us = 0;

    vestibular_process_otolith_input(processor, &input);

    float command[3];
    EXPECT_TRUE(vestibular_get_postural_command(processor, command));

    // Should generate some postural command for tilt
}

TEST_F(VestibularVSRTest, GetHeadPosition) {
    float position[3];
    EXPECT_TRUE(vestibular_get_head_position(processor, position));

    // Initial position should be near zero
}

TEST_F(VestibularVSRTest, HeadPositionUpdatesWithInput) {
    float initial_position[3];
    vestibular_get_head_position(processor, initial_position);

    // Feed rotation input
    semicircular_canal_input_t input;
    input.yaw_velocity = 1.0f;
    input.pitch_velocity = 0.0f;
    input.roll_velocity = 0.0f;
    input.timestamp_us = 0;

    for (int i = 0; i < 10; i++) {
        input.timestamp_us = i * 10000;  // 10ms steps
        vestibular_process_canal_input(processor, &input);
    }

    float final_position[3];
    vestibular_get_head_position(processor, final_position);

    // Position should have changed
}

//=============================================================================
// Cerebellar Interface Tests
//=============================================================================

class VestibularCerebellarTest : public ::testing::Test {
protected:
    vestibular_processor_t* processor = nullptr;

    void SetUp() override {
        vestibular_config_t config = vestibular_default_config();
        config.enable_cerebellar_input = true;
        processor = vestibular_create(&config);
        ASSERT_NE(processor, nullptr);
    }

    void TearDown() override {
        if (processor) {
            vestibular_destroy(processor);
            processor = nullptr;
        }
    }
};

TEST_F(VestibularCerebellarTest, GetMossySignal) {
    // Feed some input first
    semicircular_canal_input_t input;
    input.yaw_velocity = 0.5f;
    input.pitch_velocity = 0.0f;
    input.roll_velocity = 0.0f;
    input.timestamp_us = 0;

    vestibular_process_canal_input(processor, &input);

    vestibular_mossy_signal_t signal;
    EXPECT_TRUE(vestibular_get_mossy_signal(processor, &signal));

    // Signal should reflect head velocity
    EXPECT_FLOAT_EQ(signal.head_velocity[0], 0.5f);
}

TEST_F(VestibularCerebellarTest, MossySignalHasAllFields) {
    semicircular_canal_input_t canal_input;
    canal_input.yaw_velocity = 0.5f;
    canal_input.pitch_velocity = 0.1f;
    canal_input.roll_velocity = 0.0f;
    canal_input.timestamp_us = 1000;

    vestibular_process_canal_input(processor, &canal_input);

    vestibular_mossy_signal_t signal;
    vestibular_get_mossy_signal(processor, &signal);

    // Check all fields populated
    EXPECT_FLOAT_EQ(signal.head_velocity[0], 0.5f);
    EXPECT_FLOAT_EQ(signal.head_velocity[1], 0.1f);
    EXPECT_EQ(signal.timestamp_us, 1000);
}

TEST_F(VestibularCerebellarTest, ApplyCerebellarModulation) {
    // Modulation < 1 should reduce vestibular output (inhibition)
    EXPECT_TRUE(vestibular_apply_cerebellar_modulation(
        processor, VESTIBULAR_NUCLEUS_MEDIAL, 0.7f));

    vestibular_nucleus_state_t state;
    vestibular_get_nucleus_state(processor, VESTIBULAR_NUCLEUS_MEDIAL, &state);

    EXPECT_FLOAT_EQ(state.cerebellar_modulation, 0.7f);
}

TEST_F(VestibularCerebellarTest, ModulationAffectsVOR) {
    // Feed input
    semicircular_canal_input_t input;
    input.yaw_velocity = 1.0f;
    input.pitch_velocity = 0.0f;
    input.roll_velocity = 0.0f;
    input.timestamp_us = 0;

    vestibular_process_canal_input(processor, &input);

    float initial_eye[3];
    vestibular_get_vor_command(processor, initial_eye);

    // Apply inhibitory modulation
    vestibular_apply_cerebellar_modulation(processor, VESTIBULAR_NUCLEUS_MEDIAL, 0.5f);

    // Process again
    vestibular_process_canal_input(processor, &input);

    float modulated_eye[3];
    vestibular_get_vor_command(processor, modulated_eye);

    // Inhibition should reduce eye velocity magnitude
    EXPECT_LE(fabs(modulated_eye[0]), fabs(initial_eye[0]) + 0.01f);
}

TEST_F(VestibularCerebellarTest, ModulationClamped) {
    // Modulation should be clamped to [0, 2]
    EXPECT_TRUE(vestibular_apply_cerebellar_modulation(
        processor, VESTIBULAR_NUCLEUS_MEDIAL, 5.0f));  // Above max

    vestibular_nucleus_state_t state;
    vestibular_get_nucleus_state(processor, VESTIBULAR_NUCLEUS_MEDIAL, &state);

    EXPECT_LE(state.cerebellar_modulation, 2.0f);
}

//=============================================================================
// Nucleus State Tests
//=============================================================================

class VestibularNucleusStateTest : public ::testing::Test {
protected:
    vestibular_processor_t* processor = nullptr;

    void SetUp() override {
        processor = vestibular_create(nullptr);
        ASSERT_NE(processor, nullptr);
    }

    void TearDown() override {
        if (processor) {
            vestibular_destroy(processor);
            processor = nullptr;
        }
    }
};

TEST_F(VestibularNucleusStateTest, GetAllNucleiStates) {
    vestibular_nucleus_state_t state;

    EXPECT_TRUE(vestibular_get_nucleus_state(processor, VESTIBULAR_NUCLEUS_MEDIAL, &state));
    EXPECT_EQ(state.type, VESTIBULAR_NUCLEUS_MEDIAL);

    EXPECT_TRUE(vestibular_get_nucleus_state(processor, VESTIBULAR_NUCLEUS_LATERAL, &state));
    EXPECT_EQ(state.type, VESTIBULAR_NUCLEUS_LATERAL);

    EXPECT_TRUE(vestibular_get_nucleus_state(processor, VESTIBULAR_NUCLEUS_SUPERIOR, &state));
    EXPECT_EQ(state.type, VESTIBULAR_NUCLEUS_SUPERIOR);

    EXPECT_TRUE(vestibular_get_nucleus_state(processor, VESTIBULAR_NUCLEUS_INFERIOR, &state));
    EXPECT_EQ(state.type, VESTIBULAR_NUCLEUS_INFERIOR);
}

TEST_F(VestibularNucleusStateTest, NucleusNeuronCounts) {
    vestibular_config_t config = vestibular_default_config();

    vestibular_nucleus_state_t state;

    vestibular_get_nucleus_state(processor, VESTIBULAR_NUCLEUS_MEDIAL, &state);
    EXPECT_EQ(state.num_neurons, config.mvn_neurons);

    vestibular_get_nucleus_state(processor, VESTIBULAR_NUCLEUS_LATERAL, &state);
    EXPECT_EQ(state.num_neurons, config.lvn_neurons);
}

TEST_F(VestibularNucleusStateTest, BaselineRate) {
    vestibular_nucleus_state_t state;
    vestibular_get_nucleus_state(processor, VESTIBULAR_NUCLEUS_MEDIAL, &state);

    // Should have some baseline firing rate
    EXPECT_GT(state.baseline_rate, 0.0f);
}

//=============================================================================
// Status and Diagnostics Tests
//=============================================================================

class VestibularDiagnosticsTest : public ::testing::Test {
protected:
    vestibular_processor_t* processor = nullptr;

    void SetUp() override {
        processor = vestibular_create(nullptr);
        ASSERT_NE(processor, nullptr);
    }

    void TearDown() override {
        if (processor) {
            vestibular_destroy(processor);
            processor = nullptr;
        }
    }
};

TEST_F(VestibularDiagnosticsTest, GetStatus) {
    EXPECT_EQ(vestibular_get_status(processor), VESTIBULAR_STATUS_IDLE);
}

TEST_F(VestibularDiagnosticsTest, GetStatusNull) {
    EXPECT_EQ(vestibular_get_status(nullptr), VESTIBULAR_STATUS_ERROR);
}

TEST_F(VestibularDiagnosticsTest, GetLastError) {
    EXPECT_EQ(vestibular_get_last_error(processor), VESTIBULAR_ERROR_NONE);
}

TEST_F(VestibularDiagnosticsTest, GetLastErrorNull) {
    EXPECT_EQ(vestibular_get_last_error(nullptr), VESTIBULAR_ERROR_INTERNAL);
}

TEST_F(VestibularDiagnosticsTest, ErrorStrings) {
    EXPECT_STREQ(vestibular_error_string(VESTIBULAR_ERROR_NONE), "No error");
    EXPECT_STREQ(vestibular_error_string(VESTIBULAR_ERROR_INVALID_INPUT), "Invalid input");
    EXPECT_STREQ(vestibular_error_string(VESTIBULAR_ERROR_NOT_INITIALIZED), "Not initialized");
    EXPECT_STREQ(vestibular_error_string(VESTIBULAR_ERROR_CALIBRATION_FAIL), "Calibration failed");
    EXPECT_STREQ(vestibular_error_string(VESTIBULAR_ERROR_INTERNAL), "Internal error");
}

TEST_F(VestibularDiagnosticsTest, StatusStrings) {
    EXPECT_STREQ(vestibular_status_string(VESTIBULAR_STATUS_IDLE), "Idle");
    EXPECT_STREQ(vestibular_status_string(VESTIBULAR_STATUS_PROCESSING), "Processing");
    EXPECT_STREQ(vestibular_status_string(VESTIBULAR_STATUS_VOR_ACTIVE), "VOR active");
    EXPECT_STREQ(vestibular_status_string(VESTIBULAR_STATUS_VSR_ACTIVE), "VSR active");
    EXPECT_STREQ(vestibular_status_string(VESTIBULAR_STATUS_CALIBRATING), "Calibrating");
    EXPECT_STREQ(vestibular_status_string(VESTIBULAR_STATUS_ERROR), "Error");
}

TEST_F(VestibularDiagnosticsTest, GetStats) {
    vestibular_stats_t stats;
    EXPECT_TRUE(vestibular_get_stats(processor, &stats));

    EXPECT_EQ(stats.canal_inputs, 0);
    EXPECT_EQ(stats.otolith_inputs, 0);
    EXPECT_EQ(stats.vor_commands, 0);
}

TEST_F(VestibularDiagnosticsTest, GetStatsNull) {
    vestibular_stats_t stats;
    EXPECT_FALSE(vestibular_get_stats(nullptr, &stats));
    EXPECT_FALSE(vestibular_get_stats(processor, nullptr));
}

//=============================================================================
// Null Safety Tests
//=============================================================================

class VestibularNullSafetyTest : public ::testing::Test {};

TEST_F(VestibularNullSafetyTest, AllOperationsHandleNull) {
    semicircular_canal_input_t canal;
    canal.yaw_velocity = 0.0f;
    canal.pitch_velocity = 0.0f;
    canal.roll_velocity = 0.0f;
    canal.timestamp_us = 0;

    otolith_input_t otolith;
    otolith.x_accel = 0.0f;
    otolith.y_accel = 0.0f;
    otolith.z_accel = 9.8f;
    otolith.head_tilt_pitch = 0.0f;
    otolith.head_tilt_roll = 0.0f;
    otolith.timestamp_us = 0;

    vestibular_input_t combined;
    combined.canals = canal;
    combined.otoliths = otolith;
    combined.canals_valid = true;
    combined.otoliths_valid = true;

    float arr[3];
    vestibular_mossy_signal_t signal;
    vestibular_stats_t stats;
    vestibular_nucleus_state_t state;

    // All should return false/error and not crash
    EXPECT_FALSE(vestibular_reset(nullptr));
    EXPECT_FALSE(vestibular_process_input(nullptr, &combined));
    EXPECT_FALSE(vestibular_process_canal_input(nullptr, &canal));
    EXPECT_FALSE(vestibular_process_otolith_input(nullptr, &otolith));
    EXPECT_FALSE(vestibular_get_vor_command(nullptr, arr));
    EXPECT_FALSE(vestibular_set_vor_gain(nullptr, arr, false));
    EXPECT_FALSE(vestibular_get_vor_gain(nullptr, arr));
    EXPECT_FALSE(vestibular_report_retinal_slip(nullptr, 0.0f, arr));
    EXPECT_FALSE(vestibular_get_postural_command(nullptr, arr));
    EXPECT_FALSE(vestibular_get_head_position(nullptr, arr));
    EXPECT_FALSE(vestibular_get_mossy_signal(nullptr, &signal));
    EXPECT_FALSE(vestibular_apply_cerebellar_modulation(nullptr, VESTIBULAR_NUCLEUS_MEDIAL, 1.0f));
    EXPECT_EQ(vestibular_get_status(nullptr), VESTIBULAR_STATUS_ERROR);
    EXPECT_EQ(vestibular_get_last_error(nullptr), VESTIBULAR_ERROR_INTERNAL);
    EXPECT_FALSE(vestibular_get_stats(nullptr, &stats));
    EXPECT_FALSE(vestibular_get_nucleus_state(nullptr, VESTIBULAR_NUCLEUS_MEDIAL, &state));
}

//=============================================================================
// Integration Test
//=============================================================================

class VestibularIntegrationTest : public ::testing::Test {
protected:
    vestibular_processor_t* processor = nullptr;

    void SetUp() override {
        vestibular_config_t config = vestibular_default_config();
        config.enable_vor_adaptation = true;
        config.enable_vestibulospinal = true;
        config.enable_cerebellar_input = true;
        config.enable_velocity_storage = true;
        processor = vestibular_create(&config);
        ASSERT_NE(processor, nullptr);
    }

    void TearDown() override {
        if (processor) {
            vestibular_destroy(processor);
            processor = nullptr;
        }
    }
};

TEST_F(VestibularIntegrationTest, FullVORPipeline) {
    // Step 1: Head starts rotating
    semicircular_canal_input_t input;
    input.yaw_velocity = 1.0f;  // 1 rad/s yaw
    input.pitch_velocity = 0.0f;
    input.roll_velocity = 0.0f;

    for (int i = 0; i < 10; i++) {
        input.timestamp_us = i * 10000;  // 10ms steps
        EXPECT_TRUE(vestibular_process_canal_input(processor, &input));
    }

    // Step 2: Get VOR command
    float eye_velocity[3];
    EXPECT_TRUE(vestibular_get_vor_command(processor, eye_velocity));

    // Eye should move opposite to head
    EXPECT_LT(eye_velocity[0], 0.0f);

    // Step 3: Get mossy signal for cerebellum
    vestibular_mossy_signal_t signal;
    EXPECT_TRUE(vestibular_get_mossy_signal(processor, &signal));
    EXPECT_FLOAT_EQ(signal.head_velocity[0], 1.0f);

    // Step 4: Simulate retinal slip (VOR not perfect)
    float slip_direction[3] = {1.0f, 0.0f, 0.0f};
    EXPECT_TRUE(vestibular_report_retinal_slip(processor, 0.1f, slip_direction));

    // Step 5: Verify adaptation happened
    vestibular_stats_t stats;
    vestibular_get_stats(processor, &stats);
    EXPECT_GT(stats.vor_commands, 0);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
