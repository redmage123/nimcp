//=============================================================================
// test_contralateral_mapping_regression.cpp - Contralateral Mapping Regression Tests
//=============================================================================
/**
 * @file test_contralateral_mapping_regression.cpp
 * @brief Regression tests for contralateral motor/sensory mapping
 *
 * WHAT: Tests for motor mapping, sensory mapping, cross-body coordination
 * WHY:  Ensure contralateral mapping behavior is stable across versions
 * HOW:  GTest framework with accuracy and consistency checks
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>

#include "utils/nimcp_test_base.h"


#include "core/brain/hemispheric/nimcp_contralateral_mapping.h"
#include "core/brain/hemispheric/nimcp_lateralization.h"

//=============================================================================
// Test Fixtures
//=============================================================================

class ContralateralMappingRegressionTest : public NimcpTestBase {
protected:
    contralateral_system_t* left_system = nullptr;
    contralateral_system_t* right_system = nullptr;

    void SetUp() override {
        NimcpTestBase::SetUp();

        contralateral_config_t config = contralateral_default_config();
        config.total_motor_neurons = 64;
        config.total_sensory_neurons = 64;
        config.enable_somatotopy = true;
        config.enable_homunculus_weighting = true;

        left_system = contralateral_create(&config, HEMISPHERE_LEFT);
        right_system = contralateral_create(&config, HEMISPHERE_RIGHT);
    }

    void TearDown() override {
        if (left_system) {
            contralateral_destroy(left_system);
            left_system = nullptr;
        }
        if (right_system) {
            contralateral_destroy(right_system);
            right_system = nullptr;
        }
        NimcpTestBase::TearDown();
    }
};

//=============================================================================
// Motor Mapping Accuracy Tests
//=============================================================================

TEST_F(ContralateralMappingRegressionTest, LeftHemisphereControlsRightBody) {
    ASSERT_NE(left_system, nullptr);

    body_side_t controlled = contralateral_get_controlled_side(left_system);
    EXPECT_EQ(controlled, BODY_SIDE_RIGHT);

    EXPECT_TRUE(contralateral_controls_side(left_system, BODY_SIDE_RIGHT));
    EXPECT_FALSE(contralateral_controls_side(left_system, BODY_SIDE_LEFT));
}

TEST_F(ContralateralMappingRegressionTest, RightHemisphereControlsLeftBody) {
    ASSERT_NE(right_system, nullptr);

    body_side_t controlled = contralateral_get_controlled_side(right_system);
    EXPECT_EQ(controlled, BODY_SIDE_LEFT);

    EXPECT_TRUE(contralateral_controls_side(right_system, BODY_SIDE_LEFT));
    EXPECT_FALSE(contralateral_controls_side(right_system, BODY_SIDE_RIGHT));
}

TEST_F(ContralateralMappingRegressionTest, MotorOutputMapping) {
    ASSERT_NE(left_system, nullptr);

    float hemisphere_output[64];
    for (int i = 0; i < 64; i++) {
        hemisphere_output[i] = 0.5f;
    }

    motor_command_t commands[16];
    int num_commands = contralateral_process_motor(
        left_system, hemisphere_output, 64, commands, 16);

    EXPECT_GT(num_commands, 0);

    // Commands should be for the contralateral side
    for (int i = 0; i < num_commands; i++) {
        if (commands[i].side != BODY_SIDE_MIDLINE) {
            EXPECT_EQ(commands[i].side, BODY_SIDE_RIGHT);
        }
    }
}

TEST_F(ContralateralMappingRegressionTest, MotorActivationRetrieval) {
    ASSERT_NE(left_system, nullptr);

    // Process some motor output first
    float hemisphere_output[64];
    for (int i = 0; i < 64; i++) {
        hemisphere_output[i] = 0.7f;
    }

    motor_command_t commands[16];
    contralateral_process_motor(left_system, hemisphere_output, 64, commands, 16);

    // Check activation for various regions
    for (int r = 0; r < BODY_REGION_COUNT; r++) {
        float activation = contralateral_get_motor_activation(
            left_system, (body_region_t)r);

        EXPECT_GE(activation, 0.0f) << "Region " << r << " negative activation";
        EXPECT_LE(activation, 1.0f) << "Region " << r << " activation > 1";
    }
}

TEST_F(ContralateralMappingRegressionTest, MotorMappingDeterministic) {
    ASSERT_NE(left_system, nullptr);

    float hemisphere_output[64];
    for (int i = 0; i < 64; i++) {
        hemisphere_output[i] = 0.5f + 0.01f * (i % 10);
    }

    motor_command_t commands1[16], commands2[16];

    int n1 = contralateral_process_motor(left_system, hemisphere_output, 64, commands1, 16);

    // Process again with same input
    int n2 = contralateral_process_motor(left_system, hemisphere_output, 64, commands2, 16);

    EXPECT_EQ(n1, n2);

    for (int i = 0; i < n1; i++) {
        EXPECT_EQ(commands1[i].region, commands2[i].region);
        EXPECT_FLOAT_EQ(commands1[i].activation, commands2[i].activation);
    }
}

//=============================================================================
// Sensory Mapping Accuracy Tests
//=============================================================================

TEST_F(ContralateralMappingRegressionTest, SensoryInputMapping) {
    ASSERT_NE(left_system, nullptr);

    // Sensory input from right body should go to left hemisphere
    sensory_input_t inputs[4];
    inputs[0].region = BODY_REGION_HAND;
    inputs[0].side = BODY_SIDE_RIGHT;
    inputs[0].intensity = 0.8f;
    inputs[0].pressure = 0.5f;
    inputs[0].temperature = 0.0f;
    inputs[0].pain = 0.0f;
    inputs[0].proprioception = 0.3f;

    inputs[1].region = BODY_REGION_FOOT;
    inputs[1].side = BODY_SIDE_RIGHT;
    inputs[1].intensity = 0.6f;
    inputs[1].pressure = 0.4f;
    inputs[1].temperature = 0.0f;
    inputs[1].pain = 0.0f;
    inputs[1].proprioception = 0.2f;

    float hemisphere_input[64];
    int num_activations = contralateral_process_sensory(
        left_system, inputs, 2, hemisphere_input, 64);

    EXPECT_GT(num_activations, 0);

    // Should have non-zero activations
    bool has_activation = false;
    for (int i = 0; i < num_activations; i++) {
        if (hemisphere_input[i] > 0.0f) {
            has_activation = true;
            break;
        }
    }
    EXPECT_TRUE(has_activation);
}

TEST_F(ContralateralMappingRegressionTest, SensoryActivationRetrieval) {
    ASSERT_NE(left_system, nullptr);

    // Process some sensory input first
    sensory_input_t inputs[2];
    inputs[0].region = BODY_REGION_FINGERS;
    inputs[0].side = BODY_SIDE_RIGHT;
    inputs[0].intensity = 0.9f;
    inputs[0].pressure = 0.7f;
    inputs[0].temperature = 0.0f;
    inputs[0].pain = 0.0f;
    inputs[0].proprioception = 0.5f;

    float hemisphere_input[64];
    contralateral_process_sensory(left_system, inputs, 1, hemisphere_input, 64);

    // Check activation for various regions
    for (int r = 0; r < BODY_REGION_COUNT; r++) {
        float activation = contralateral_get_sensory_activation(
            left_system, (body_region_t)r);

        EXPECT_GE(activation, 0.0f) << "Region " << r << " negative activation";
        EXPECT_LE(activation, 1.0f) << "Region " << r << " activation > 1";
    }
}

TEST_F(ContralateralMappingRegressionTest, SensoryMappingDeterministic) {
    ASSERT_NE(left_system, nullptr);

    sensory_input_t inputs[2];
    inputs[0].region = BODY_REGION_ARM;
    inputs[0].side = BODY_SIDE_RIGHT;
    inputs[0].intensity = 0.6f;
    inputs[0].pressure = 0.4f;
    inputs[0].temperature = 0.0f;
    inputs[0].pain = 0.0f;
    inputs[0].proprioception = 0.3f;

    float output1[64], output2[64];

    int n1 = contralateral_process_sensory(left_system, inputs, 1, output1, 64);
    int n2 = contralateral_process_sensory(left_system, inputs, 1, output2, 64);

    EXPECT_EQ(n1, n2);

    for (int i = 0; i < n1; i++) {
        EXPECT_FLOAT_EQ(output1[i], output2[i]) << "Mismatch at " << i;
    }
}

//=============================================================================
// Cross-Body Coordination Consistency Tests
//=============================================================================

TEST_F(ContralateralMappingRegressionTest, SymmetricHemisphereMapping) {
    ASSERT_NE(left_system, nullptr);
    ASSERT_NE(right_system, nullptr);

    // Left hemisphere controls right body
    EXPECT_EQ(contralateral_get_controlled_side(left_system), BODY_SIDE_RIGHT);

    // Right hemisphere controls left body
    EXPECT_EQ(contralateral_get_controlled_side(right_system), BODY_SIDE_LEFT);
}

TEST_F(ContralateralMappingRegressionTest, MidlineHandledCorrectly) {
    ASSERT_NE(left_system, nullptr);
    ASSERT_NE(right_system, nullptr);

    // Midline should be controlled by both
    // This depends on implementation - typically partial control
    sensory_input_t input;
    input.region = BODY_REGION_TRUNK;
    input.side = BODY_SIDE_MIDLINE;
    input.intensity = 0.5f;
    input.pressure = 0.3f;
    input.temperature = 0.0f;
    input.pain = 0.0f;
    input.proprioception = 0.2f;

    float left_out[64], right_out[64];

    int left_n = contralateral_process_sensory(left_system, &input, 1, left_out, 64);
    int right_n = contralateral_process_sensory(right_system, &input, 1, right_out, 64);

    // Both hemispheres should process midline input
    EXPECT_GE(left_n, 0);
    EXPECT_GE(right_n, 0);
}

TEST_F(ContralateralMappingRegressionTest, CrossingFractionRespected) {
    contralateral_config_t config = contralateral_default_config();

    // Default crossing fraction should be 0.90 (90% contralateral)
    EXPECT_FLOAT_EQ(config.crossing_fraction, CONTRALATERAL_CROSSING_FRACTION);
    EXPECT_FLOAT_EQ(CONTRALATERAL_CROSSING_FRACTION, 0.90f);
}

TEST_F(ContralateralMappingRegressionTest, BiHemisphericMotorCoordination) {
    ASSERT_NE(left_system, nullptr);
    ASSERT_NE(right_system, nullptr);

    float left_output[64], right_output[64];
    for (int i = 0; i < 64; i++) {
        left_output[i] = 0.5f;
        right_output[i] = 0.5f;
    }

    motor_command_t left_commands[16], right_commands[16];

    int left_n = contralateral_process_motor(left_system, left_output, 64, left_commands, 16);
    int right_n = contralateral_process_motor(right_system, right_output, 64, right_commands, 16);

    // Both should produce commands
    EXPECT_GT(left_n, 0);
    EXPECT_GT(right_n, 0);

    // Left system commands should be for right body
    for (int i = 0; i < left_n; i++) {
        if (left_commands[i].side != BODY_SIDE_MIDLINE) {
            EXPECT_EQ(left_commands[i].side, BODY_SIDE_RIGHT);
        }
    }

    // Right system commands should be for left body
    for (int i = 0; i < right_n; i++) {
        if (right_commands[i].side != BODY_SIDE_MIDLINE) {
            EXPECT_EQ(right_commands[i].side, BODY_SIDE_LEFT);
        }
    }
}

//=============================================================================
// Somatotopy Tests
//=============================================================================

TEST_F(ContralateralMappingRegressionTest, CortexFractionsSumToOne) {
    ASSERT_NE(left_system, nullptr);

    float motor_total = 0.0f;
    float sensory_total = 0.0f;

    for (int r = 0; r < BODY_REGION_COUNT; r++) {
        motor_total += contralateral_get_cortex_fraction(
            left_system, (body_region_t)r, true);
        sensory_total += contralateral_get_cortex_fraction(
            left_system, (body_region_t)r, false);
    }

    // Should sum to approximately 1.0
    EXPECT_NEAR(motor_total, 1.0f, 0.01f);
    EXPECT_NEAR(sensory_total, 1.0f, 0.01f);
}

TEST_F(ContralateralMappingRegressionTest, HandFingerLargerCortexRepresentation) {
    ASSERT_NE(left_system, nullptr);

    // Hand and fingers should have larger cortical representation than trunk
    float hand_motor = contralateral_get_cortex_fraction(
        left_system, BODY_REGION_HAND, true);
    float fingers_motor = contralateral_get_cortex_fraction(
        left_system, BODY_REGION_FINGERS, true);
    float trunk_motor = contralateral_get_cortex_fraction(
        left_system, BODY_REGION_TRUNK, true);

    EXPECT_GT(hand_motor + fingers_motor, trunk_motor);
}

TEST_F(ContralateralMappingRegressionTest, FaceLargerCortexRepresentation) {
    ASSERT_NE(left_system, nullptr);

    // Face and lips should have larger representation than leg
    float face_sensory = contralateral_get_cortex_fraction(
        left_system, BODY_REGION_FACE, false);
    float lips_sensory = contralateral_get_cortex_fraction(
        left_system, BODY_REGION_LIPS, false);
    float leg_sensory = contralateral_get_cortex_fraction(
        left_system, BODY_REGION_LEG, false);

    EXPECT_GT(face_sensory + lips_sensory, leg_sensory);
}

//=============================================================================
// Backward Compatibility Tests
//=============================================================================

TEST_F(ContralateralMappingRegressionTest, DefaultConfigStable) {
    contralateral_config_t config = contralateral_default_config();

    EXPECT_FLOAT_EQ(config.crossing_fraction, 0.90f);
    EXPECT_TRUE(config.enable_somatotopy);
    EXPECT_TRUE(config.enable_homunculus_weighting);
}

TEST_F(ContralateralMappingRegressionTest, RegionNamesStable) {
    EXPECT_STREQ(contralateral_region_to_string(BODY_REGION_FOOT), "Foot");
    EXPECT_STREQ(contralateral_region_to_string(BODY_REGION_LEG), "Leg");
    EXPECT_STREQ(contralateral_region_to_string(BODY_REGION_TRUNK), "Trunk");
    EXPECT_STREQ(contralateral_region_to_string(BODY_REGION_SHOULDER), "Shoulder");
    EXPECT_STREQ(contralateral_region_to_string(BODY_REGION_ARM), "Arm");
    EXPECT_STREQ(contralateral_region_to_string(BODY_REGION_HAND), "Hand");
    EXPECT_STREQ(contralateral_region_to_string(BODY_REGION_FINGERS), "Fingers");
    EXPECT_STREQ(contralateral_region_to_string(BODY_REGION_NECK), "Neck");
    EXPECT_STREQ(contralateral_region_to_string(BODY_REGION_FACE), "Face");
    EXPECT_STREQ(contralateral_region_to_string(BODY_REGION_LIPS), "Lips");
    EXPECT_STREQ(contralateral_region_to_string(BODY_REGION_TONGUE), "Tongue");
    EXPECT_STREQ(contralateral_region_to_string(BODY_REGION_THROAT), "Throat");
}

TEST_F(ContralateralMappingRegressionTest, BodyRegionCountStable) {
    EXPECT_EQ(BODY_REGION_COUNT, 12);
}

//=============================================================================
// Null Safety and Stress Tests
//=============================================================================

TEST_F(ContralateralMappingRegressionTest, NullPointerSafety) {
    // These should not crash
    contralateral_destroy(nullptr);
    contralateral_get_controlled_side(nullptr);
    contralateral_controls_side(nullptr, BODY_SIDE_LEFT);
    contralateral_get_motor_activation(nullptr, BODY_REGION_HAND);
    contralateral_get_sensory_activation(nullptr, BODY_REGION_HAND);
    contralateral_get_cortex_fraction(nullptr, BODY_REGION_HAND, true);

    float buffer[64];
    motor_command_t commands[16];
    sensory_input_t inputs[4];

    contralateral_process_motor(nullptr, buffer, 64, commands, 16);
    contralateral_process_sensory(nullptr, inputs, 4, buffer, 64);
}

TEST_F(ContralateralMappingRegressionTest, CreateDestroyNoLeak) {
    for (int i = 0; i < 100; i++) {
        contralateral_config_t config = contralateral_default_config();
        config.total_motor_neurons = 32;
        config.total_sensory_neurons = 32;

        contralateral_system_t* sys = contralateral_create(&config, HEMISPHERE_LEFT);
        ASSERT_NE(sys, nullptr);
        contralateral_destroy(sys);
    }
}

TEST_F(ContralateralMappingRegressionTest, RepeatedProcessingStable) {
    ASSERT_NE(left_system, nullptr);

    float output[64];
    for (int i = 0; i < 64; i++) output[i] = 0.5f;

    motor_command_t commands[16];
    sensory_input_t input;
    input.region = BODY_REGION_HAND;
    input.side = BODY_SIDE_RIGHT;
    input.intensity = 0.5f;
    input.pressure = 0.3f;
    input.temperature = 0.0f;
    input.pain = 0.0f;
    input.proprioception = 0.2f;

    float sensory_out[64];

    for (int i = 0; i < 1000; i++) {
        int motor_n = contralateral_process_motor(left_system, output, 64, commands, 16);
        int sensory_n = contralateral_process_sensory(left_system, &input, 1, sensory_out, 64);

        EXPECT_GE(motor_n, 0) << "Motor processing failed at iteration " << i;
        EXPECT_GE(sensory_n, 0) << "Sensory processing failed at iteration " << i;
    }
}
