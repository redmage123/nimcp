/**
 * @file test_vestibular.cpp
 * @brief Unit tests for vestibular processor
 *
 * Tests:
 * - Lifecycle (create, destroy, reset)
 * - Canal input processing
 * - Otolith input processing
 * - VOR generation and adaptation
 * - Vestibulospinal reflex
 * - Mossy fiber signal generation
 * - Cerebellar modulation
 *
 * @version Phase 3: Vestibular System Integration
 * @date 2026-01-03
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

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
    config.initial_vor_gain = 0.9f;
    processor = vestibular_create(&config);
    ASSERT_NE(processor, nullptr);

    float gain[3];
    EXPECT_TRUE(vestibular_get_vor_gain(processor, gain));
    EXPECT_FLOAT_EQ(gain[0], 0.9f);
}

TEST_F(VestibularLifecycleTest, DefaultConfigValues) {
    vestibular_config_t config = vestibular_default_config();

    EXPECT_GT(config.mvn_neurons, 0);
    EXPECT_GT(config.lvn_neurons, 0);
    EXPECT_GT(config.svn_neurons, 0);
    EXPECT_GT(config.ivn_neurons, 0);
    EXPECT_FLOAT_EQ(config.initial_vor_gain, VESTIBULAR_DEFAULT_VOR_GAIN);
    EXPECT_TRUE(config.enable_vor_adaptation);
    EXPECT_TRUE(config.enable_velocity_storage);
    EXPECT_TRUE(config.enable_vestibulospinal);
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

//=============================================================================
// Canal Input Tests
//=============================================================================

class VestibularCanalTest : public ::testing::Test {
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

TEST_F(VestibularCanalTest, ProcessCanalInput) {
    semicircular_canal_input_t input;
    input.yaw_velocity = 1.0f;    // rad/s
    input.pitch_velocity = 0.0f;
    input.roll_velocity = 0.0f;
    input.timestamp_us = 0;

    EXPECT_TRUE(vestibular_process_canal_input(processor, &input));
}

TEST_F(VestibularCanalTest, ProcessMultipleCanalInputs) {
    for (int i = 0; i < 100; i++) {
        semicircular_canal_input_t input;
        input.yaw_velocity = sinf((float)i * 0.1f);
        input.pitch_velocity = 0.0f;
        input.roll_velocity = 0.0f;
        input.timestamp_us = i * 1000;

        EXPECT_TRUE(vestibular_process_canal_input(processor, &input));
    }

    vestibular_stats_t stats;
    vestibular_get_stats(processor, &stats);
    EXPECT_EQ(stats.canal_inputs, 100);
}

TEST_F(VestibularCanalTest, CanalInputGeneratesVOR) {
    semicircular_canal_input_t input;
    input.yaw_velocity = 1.0f;
    input.pitch_velocity = 0.0f;
    input.roll_velocity = 0.0f;
    input.timestamp_us = 0;

    vestibular_process_canal_input(processor, &input);

    float eye_velocity[3];
    EXPECT_TRUE(vestibular_get_vor_command(processor, eye_velocity));

    // VOR should produce opposite eye movement
    EXPECT_LT(eye_velocity[0], 0.0f);  // Opposite to yaw
}

TEST_F(VestibularCanalTest, NullInput) {
    EXPECT_FALSE(vestibular_process_canal_input(processor, nullptr));
}

//=============================================================================
// Otolith Input Tests
//=============================================================================

class VestibularOtolithTest : public ::testing::Test {
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

TEST_F(VestibularOtolithTest, ProcessOtolithInput) {
    otolith_input_t input;
    input.x_accel = 0.0f;
    input.y_accel = 0.0f;
    input.z_accel = 9.81f;  // Upright
    input.head_tilt_pitch = 0.0f;
    input.head_tilt_roll = 0.0f;
    input.timestamp_us = 0;

    EXPECT_TRUE(vestibular_process_otolith_input(processor, &input));
}

TEST_F(VestibularOtolithTest, HeadTiltDetection) {
    otolith_input_t input;
    input.x_accel = 0.0f;
    input.y_accel = 2.0f;   // Tilt to side
    input.z_accel = 9.81f;
    input.head_tilt_pitch = 0.0f;
    input.head_tilt_roll = 0.2f;  // ~11 degrees
    input.timestamp_us = 0;

    vestibular_process_otolith_input(processor, &input);

    float postural[3];
    EXPECT_TRUE(vestibular_get_postural_command(processor, postural));

    // Should have some corrective command
    // Roll correction is in index 1 (pitch=0, roll=1, vertical=2)
    EXPECT_NE(postural[1], 0.0f);  // Roll correction
}

TEST_F(VestibularOtolithTest, StatsTrackOtolithInputs) {
    for (int i = 0; i < 50; i++) {
        otolith_input_t input;
        input.x_accel = 0.1f * sinf((float)i * 0.1f);
        input.y_accel = 0.0f;
        input.z_accel = 9.81f;
        input.head_tilt_pitch = 0.0f;
        input.head_tilt_roll = 0.0f;
        input.timestamp_us = i * 1000;

        vestibular_process_otolith_input(processor, &input);
    }

    vestibular_stats_t stats;
    vestibular_get_stats(processor, &stats);
    EXPECT_EQ(stats.otolith_inputs, 50);
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

TEST_F(VestibularVORTest, VORGainDefault) {
    float gain[3];
    EXPECT_TRUE(vestibular_get_vor_gain(processor, gain));

    EXPECT_FLOAT_EQ(gain[0], VESTIBULAR_DEFAULT_VOR_GAIN);
    EXPECT_FLOAT_EQ(gain[1], VESTIBULAR_DEFAULT_VOR_GAIN);
    EXPECT_FLOAT_EQ(gain[2], VESTIBULAR_DEFAULT_VOR_GAIN);
}

TEST_F(VestibularVORTest, SetVORGain) {
    float new_gain = 1.2f;
    EXPECT_TRUE(vestibular_set_vor_gain(processor, &new_gain, false));

    float gain[3];
    vestibular_get_vor_gain(processor, gain);
    EXPECT_FLOAT_EQ(gain[0], 1.2f);
    EXPECT_FLOAT_EQ(gain[1], 1.2f);
    EXPECT_FLOAT_EQ(gain[2], 1.2f);
}

TEST_F(VestibularVORTest, SetVORGainPerAxis) {
    float gains[3] = {0.8f, 1.0f, 1.2f};
    EXPECT_TRUE(vestibular_set_vor_gain(processor, gains, true));

    float retrieved[3];
    vestibular_get_vor_gain(processor, retrieved);
    EXPECT_FLOAT_EQ(retrieved[0], 0.8f);
    EXPECT_FLOAT_EQ(retrieved[1], 1.0f);
    EXPECT_FLOAT_EQ(retrieved[2], 1.2f);
}

TEST_F(VestibularVORTest, VORGainClamped) {
    float too_high = 3.0f;
    vestibular_set_vor_gain(processor, &too_high, false);

    float gain[3];
    vestibular_get_vor_gain(processor, gain);
    EXPECT_LE(gain[0], VESTIBULAR_MAX_VOR_GAIN);

    float too_low = 0.1f;
    vestibular_set_vor_gain(processor, &too_low, false);
    vestibular_get_vor_gain(processor, gain);
    EXPECT_GE(gain[0], VESTIBULAR_MIN_VOR_GAIN);
}

TEST_F(VestibularVORTest, VORAdaptation) {
    // First process a canal input to set head velocity
    semicircular_canal_input_t canal_input;
    canal_input.yaw_velocity = 1.0f;
    canal_input.pitch_velocity = 0.0f;
    canal_input.roll_velocity = 0.0f;
    canal_input.timestamp_us = 0;
    vestibular_process_canal_input(processor, &canal_input);

    float initial_gain[3];
    vestibular_get_vor_gain(processor, initial_gain);

    // Report positive retinal slip (eyes not moving enough)
    float direction[3] = {1.0f, 0.0f, 0.0f};  // Yaw direction
    vestibular_report_retinal_slip(processor, 0.5f, direction);

    float adapted_gain[3];
    vestibular_get_vor_gain(processor, adapted_gain);

    // Gain should increase to compensate
    EXPECT_GT(adapted_gain[0], initial_gain[0]);
}

TEST_F(VestibularVORTest, VORCommandOppositeToHead) {
    semicircular_canal_input_t input;
    input.yaw_velocity = 2.0f;   // Rightward head turn
    input.pitch_velocity = 0.0f;
    input.roll_velocity = 0.0f;
    input.timestamp_us = 0;

    vestibular_process_canal_input(processor, &input);

    float eye_velocity[3];
    vestibular_get_vor_command(processor, eye_velocity);

    // Eye velocity should be opposite to head velocity
    EXPECT_LT(eye_velocity[0] * input.yaw_velocity, 0.0f);
}

//=============================================================================
// Vestibulospinal Tests
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

TEST_F(VestibularVSRTest, PosturalCommand) {
    // Tilt input
    otolith_input_t input;
    input.x_accel = 0.0f;
    input.y_accel = 1.0f;
    input.z_accel = 9.81f;
    input.head_tilt_pitch = 0.0f;
    input.head_tilt_roll = 0.1f;
    input.timestamp_us = 0;

    vestibular_process_otolith_input(processor, &input);

    float postural[3];
    EXPECT_TRUE(vestibular_get_postural_command(processor, postural));
}

TEST_F(VestibularVSRTest, HeadPositionEstimate) {
    // Process multiple inputs
    for (int i = 0; i < 10; i++) {
        semicircular_canal_input_t input;
        input.yaw_velocity = 0.1f;
        input.pitch_velocity = 0.0f;
        input.roll_velocity = 0.0f;
        input.timestamp_us = i * 10000;

        vestibular_process_canal_input(processor, &input);
    }

    float position[3];
    EXPECT_TRUE(vestibular_get_head_position(processor, position));

    // After rightward rotation, yaw should be positive
    EXPECT_GT(position[0], 0.0f);
}

//=============================================================================
// Mossy Fiber Signal Tests
//=============================================================================

class VestibularMossySignalTest : public ::testing::Test {
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

TEST_F(VestibularMossySignalTest, GenerateMossySignal) {
    semicircular_canal_input_t input;
    input.yaw_velocity = 1.0f;
    input.pitch_velocity = 0.5f;
    input.roll_velocity = 0.0f;
    input.timestamp_us = 0;

    vestibular_process_canal_input(processor, &input);

    vestibular_mossy_signal_t signal;
    EXPECT_TRUE(vestibular_get_mossy_signal(processor, &signal));

    EXPECT_FLOAT_EQ(signal.head_velocity[0], 1.0f);
    EXPECT_FLOAT_EQ(signal.head_velocity[1], 0.5f);
}

TEST_F(VestibularMossySignalTest, MossySignalIncludesRetinalSlip) {
    // First generate VOR
    semicircular_canal_input_t input;
    input.yaw_velocity = 1.0f;
    input.pitch_velocity = 0.0f;
    input.roll_velocity = 0.0f;
    input.timestamp_us = 0;
    vestibular_process_canal_input(processor, &input);

    // Report retinal slip
    float direction[3] = {1.0f, 0.0f, 0.0f};
    vestibular_report_retinal_slip(processor, 0.3f, direction);

    vestibular_mossy_signal_t signal;
    vestibular_get_mossy_signal(processor, &signal);

    EXPECT_FLOAT_EQ(signal.retinal_slip, 0.3f);
}

//=============================================================================
// Cerebellar Modulation Tests
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

TEST_F(VestibularCerebellarTest, ApplyModulation) {
    // Apply inhibitory modulation (< 1.0)
    EXPECT_TRUE(vestibular_apply_cerebellar_modulation(
        processor, VESTIBULAR_NUCLEUS_MEDIAL, 0.8f));

    vestibular_nucleus_state_t state;
    vestibular_get_nucleus_state(processor, VESTIBULAR_NUCLEUS_MEDIAL, &state);
    EXPECT_FLOAT_EQ(state.cerebellar_modulation, 0.8f);
}

TEST_F(VestibularCerebellarTest, ModulationAffectsVOR) {
    // Generate baseline VOR
    semicircular_canal_input_t input;
    input.yaw_velocity = 1.0f;
    input.pitch_velocity = 0.0f;
    input.roll_velocity = 0.0f;
    input.timestamp_us = 0;
    vestibular_process_canal_input(processor, &input);

    float baseline_eye[3];
    vestibular_get_vor_command(processor, baseline_eye);

    // Apply inhibitory modulation
    vestibular_apply_cerebellar_modulation(processor, VESTIBULAR_NUCLEUS_MEDIAL, 0.5f);

    // Process same input
    vestibular_process_canal_input(processor, &input);

    float modulated_eye[3];
    vestibular_get_vor_command(processor, modulated_eye);

    // Modulated VOR should be reduced
    EXPECT_LT(fabsf(modulated_eye[0]), fabsf(baseline_eye[0]));
}

//=============================================================================
// Nucleus State Tests
//=============================================================================

class VestibularNucleusTest : public ::testing::Test {
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

TEST_F(VestibularNucleusTest, GetAllNucleiStates) {
    for (int i = 0; i < VESTIBULAR_NUM_NUCLEI; i++) {
        vestibular_nucleus_state_t state;
        EXPECT_TRUE(vestibular_get_nucleus_state(
            processor, (vestibular_nucleus_type_t)i, &state));
        EXPECT_EQ(state.type, (vestibular_nucleus_type_t)i);
        EXPECT_GT(state.num_neurons, 0);
    }
}

TEST_F(VestibularNucleusTest, InvalidNucleusType) {
    vestibular_nucleus_state_t state;
    EXPECT_FALSE(vestibular_get_nucleus_state(
        processor, (vestibular_nucleus_type_t)100, &state));
}

TEST_F(VestibularNucleusTest, NucleiHaveBaselineFiring) {
    vestibular_nucleus_state_t state;
    vestibular_get_nucleus_state(processor, VESTIBULAR_NUCLEUS_MEDIAL, &state);

    // Vestibular neurons have ~100 Hz baseline
    EXPECT_GT(state.baseline_rate, 50.0f);
    EXPECT_LT(state.baseline_rate, 200.0f);
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

TEST_F(VestibularDiagnosticsTest, GetLastError) {
    EXPECT_EQ(vestibular_get_last_error(processor), VESTIBULAR_ERROR_NONE);
}

TEST_F(VestibularDiagnosticsTest, ErrorStrings) {
    EXPECT_STREQ(vestibular_error_string(VESTIBULAR_ERROR_NONE), "No error");
    EXPECT_STREQ(vestibular_error_string(VESTIBULAR_ERROR_INVALID_INPUT), "Invalid input");
    EXPECT_STREQ(vestibular_error_string(VESTIBULAR_ERROR_NOT_INITIALIZED), "Not initialized");
}

TEST_F(VestibularDiagnosticsTest, StatusStrings) {
    EXPECT_STREQ(vestibular_status_string(VESTIBULAR_STATUS_IDLE), "Idle");
    EXPECT_STREQ(vestibular_status_string(VESTIBULAR_STATUS_VOR_ACTIVE), "VOR active");
    EXPECT_STREQ(vestibular_status_string(VESTIBULAR_STATUS_VSR_ACTIVE), "VSR active");
}

TEST_F(VestibularDiagnosticsTest, GetStats) {
    vestibular_stats_t stats;
    EXPECT_TRUE(vestibular_get_stats(processor, &stats));
}

//=============================================================================
// Null Safety Tests
//=============================================================================

class VestibularNullSafetyTest : public ::testing::Test {};

TEST_F(VestibularNullSafetyTest, AllOperationsHandleNull) {
    semicircular_canal_input_t canal;
    memset(&canal, 0, sizeof(canal));

    otolith_input_t otolith;
    memset(&otolith, 0, sizeof(otolith));

    vestibular_input_t combined;
    memset(&combined, 0, sizeof(combined));

    float vals[3];
    vestibular_nucleus_state_t state;
    vestibular_mossy_signal_t signal;
    vestibular_stats_t stats;

    EXPECT_FALSE(vestibular_reset(nullptr));
    EXPECT_FALSE(vestibular_process_input(nullptr, &combined));
    EXPECT_FALSE(vestibular_process_canal_input(nullptr, &canal));
    EXPECT_FALSE(vestibular_process_otolith_input(nullptr, &otolith));
    EXPECT_FALSE(vestibular_get_vor_command(nullptr, vals));
    EXPECT_FALSE(vestibular_set_vor_gain(nullptr, vals, false));
    EXPECT_FALSE(vestibular_get_vor_gain(nullptr, vals));
    EXPECT_FALSE(vestibular_get_postural_command(nullptr, vals));
    EXPECT_FALSE(vestibular_get_head_position(nullptr, vals));
    EXPECT_FALSE(vestibular_get_mossy_signal(nullptr, &signal));
    EXPECT_FALSE(vestibular_apply_cerebellar_modulation(nullptr, VESTIBULAR_NUCLEUS_MEDIAL, 1.0f));
    EXPECT_EQ(vestibular_get_status(nullptr), VESTIBULAR_STATUS_ERROR);
    EXPECT_EQ(vestibular_get_last_error(nullptr), VESTIBULAR_ERROR_INTERNAL);
    EXPECT_FALSE(vestibular_get_stats(nullptr, &stats));
    EXPECT_FALSE(vestibular_get_nucleus_state(nullptr, VESTIBULAR_NUCLEUS_MEDIAL, &state));
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
