//=============================================================================
// test_bg_striosome_matrix.cpp - Striosome-Matrix Unit Tests
//=============================================================================
/**
 * @file test_bg_striosome_matrix.cpp
 * @brief Unit tests for striosome-matrix compartmentalization
 *
 * Tests striosome value/motivation processing and matrix motor output
 * with proper bidirectional data flow validation.
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>

#include "core/brain/subcortical/nimcp_bg_striosome_matrix.h"

//=============================================================================
// Striosome-Matrix System Tests
//=============================================================================

class StriosomeMatrixTest : public ::testing::Test {
protected:
    bgsm_system_t* system = nullptr;
    bgsm_config_t config;

    void SetUp() override {
        bgsm_default_config(&config);
        config.num_striosomes = 8;
        config.num_matrix_zones = 16;
        system = bgsm_create(&config);
    }

    void TearDown() override {
        if (system) {
            bgsm_destroy(system);
        }
    }
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(StriosomeMatrixTest, CreateDestroy) {
    ASSERT_NE(system, nullptr);
}

TEST_F(StriosomeMatrixTest, CreateWithNullConfig) {
    bgsm_system_t* s = bgsm_create(nullptr);
    ASSERT_NE(s, nullptr);
    bgsm_destroy(s);
}

TEST_F(StriosomeMatrixTest, DefaultConfig) {
    bgsm_config_t cfg;
    bgsm_default_config(&cfg);

    EXPECT_GT(cfg.num_striosomes, 0u);
    EXPECT_GT(cfg.num_matrix_zones, 0u);
    EXPECT_FLOAT_EQ(cfg.striosome_ratio, BGSM_STRIOSOME_RATIO);
}

TEST_F(StriosomeMatrixTest, Reset) {
    // Set some inputs to change state
    std::vector<float> limbic_input(config.num_striosomes, 0.8f);
    bgsm_set_striosome_input(system, BGSM_INPUT_LIMBIC, limbic_input.data());
    bgsm_process_striosomes(system);

    ASSERT_EQ(bgsm_reset(system), 0);

    // After reset, SNc modulation should be near zero
    float snc = bgsm_get_snc_modulation(system);
    EXPECT_NEAR(snc, 0.0f, 0.1f);
}

//=============================================================================
// Striosome Tests
//=============================================================================

TEST_F(StriosomeMatrixTest, StriosomeLimbicInput) {
    std::vector<float> limbic_input(config.num_striosomes, 0.7f);

    int ret = bgsm_set_striosome_input(system, BGSM_INPUT_LIMBIC, limbic_input.data());
    ASSERT_EQ(ret, 0);

    ret = bgsm_process_striosomes(system);
    ASSERT_EQ(ret, 0);

    // Check striosome activation increased
    float activation = bgsm_get_striosome_activation(system, 0);
    EXPECT_GT(activation, 0.0f);
    EXPECT_LE(activation, 1.0f);
}

TEST_F(StriosomeMatrixTest, StriosomeMPFCInput) {
    std::vector<float> mpfc_input(config.num_striosomes, 0.6f);

    int ret = bgsm_set_striosome_input(system, BGSM_INPUT_MPFC, mpfc_input.data());
    ASSERT_EQ(ret, 0);

    ret = bgsm_process_striosomes(system);
    ASSERT_EQ(ret, 0);
}

TEST_F(StriosomeMatrixTest, StriosomeAmygdalaInput) {
    std::vector<float> amygdala_input(config.num_striosomes, 0.8f);

    int ret = bgsm_set_striosome_input(system, BGSM_INPUT_AMYGDALA, amygdala_input.data());
    ASSERT_EQ(ret, 0);

    ret = bgsm_process_striosomes(system);
    ASSERT_EQ(ret, 0);
}

TEST_F(StriosomeMatrixTest, StriosomeSNcModulation) {
    // High limbic/emotional input should produce positive SNc modulation
    std::vector<float> high_input(config.num_striosomes, 0.9f);
    bgsm_set_striosome_input(system, BGSM_INPUT_LIMBIC, high_input.data());
    bgsm_set_striosome_input(system, BGSM_INPUT_AMYGDALA, high_input.data());
    bgsm_process_striosomes(system);

    float snc_high = bgsm_get_snc_modulation(system);

    // Low input should produce lower modulation
    bgsm_reset(system);
    std::vector<float> low_input(config.num_striosomes, 0.1f);
    bgsm_set_striosome_input(system, BGSM_INPUT_LIMBIC, low_input.data());
    bgsm_process_striosomes(system);

    float snc_low = bgsm_get_snc_modulation(system);

    EXPECT_GT(snc_high, snc_low);
}

TEST_F(StriosomeMatrixTest, GetMotivation) {
    std::vector<float> limbic_input(config.num_striosomes, 0.7f);
    bgsm_set_striosome_input(system, BGSM_INPUT_LIMBIC, limbic_input.data());
    bgsm_process_striosomes(system);

    float motivation = bgsm_get_motivation(system);
    EXPECT_GE(motivation, 0.0f);
    EXPECT_LE(motivation, 1.0f);
}

//=============================================================================
// Matrix Tests
//=============================================================================

TEST_F(StriosomeMatrixTest, MatrixMotorInput) {
    std::vector<float> motor_input(config.num_matrix_zones, 0.5f);

    int ret = bgsm_set_matrix_input(system, BGSM_INPUT_MOTOR, motor_input.data());
    ASSERT_EQ(ret, 0);

    ret = bgsm_process_matrix(system);
    ASSERT_EQ(ret, 0);
}

TEST_F(StriosomeMatrixTest, MatrixPreMotorInput) {
    std::vector<float> premotor_input(config.num_matrix_zones, 0.6f);

    int ret = bgsm_set_matrix_input(system, BGSM_INPUT_PREMOTOR, premotor_input.data());
    ASSERT_EQ(ret, 0);

    ret = bgsm_process_matrix(system);
    ASSERT_EQ(ret, 0);
}

TEST_F(StriosomeMatrixTest, MatrixDopamineModulation) {
    std::vector<float> motor_input(config.num_matrix_zones, 0.5f);
    bgsm_set_matrix_input(system, BGSM_INPUT_MOTOR, motor_input.data());

    // High dopamine enhances D1, suppresses D2
    bgsm_set_matrix_dopamine(system, 0.9f);
    bgsm_process_matrix(system);

    float d1_high_da = bgsm_get_d1_output(system, 0);
    float d2_high_da = bgsm_get_d2_output(system, 0);

    // Low dopamine
    bgsm_reset(system);
    bgsm_set_matrix_input(system, BGSM_INPUT_MOTOR, motor_input.data());
    bgsm_set_matrix_dopamine(system, 0.1f);
    bgsm_process_matrix(system);

    float d1_low_da = bgsm_get_d1_output(system, 0);
    float d2_low_da = bgsm_get_d2_output(system, 0);

    // D1 should be higher with more dopamine
    EXPECT_GT(d1_high_da, d1_low_da);
    // D2 should be lower with more dopamine
    EXPECT_LT(d2_high_da, d2_low_da);
}

TEST_F(StriosomeMatrixTest, GetAllD1Output) {
    std::vector<float> motor_input(config.num_matrix_zones, 0.5f);
    bgsm_set_matrix_input(system, BGSM_INPUT_MOTOR, motor_input.data());
    bgsm_set_matrix_dopamine(system, 0.5f);
    bgsm_process_matrix(system);

    std::vector<float> d1_output(config.num_matrix_zones, 0.0f);
    int ret = bgsm_get_all_d1_output(system, d1_output.data());
    ASSERT_EQ(ret, 0);

    for (uint32_t i = 0; i < config.num_matrix_zones; i++) {
        EXPECT_GE(d1_output[i], 0.0f);
        EXPECT_LE(d1_output[i], 1.0f);
    }
}

TEST_F(StriosomeMatrixTest, GetAllD2Output) {
    std::vector<float> motor_input(config.num_matrix_zones, 0.5f);
    bgsm_set_matrix_input(system, BGSM_INPUT_MOTOR, motor_input.data());
    bgsm_set_matrix_dopamine(system, 0.5f);
    bgsm_process_matrix(system);

    std::vector<float> d2_output(config.num_matrix_zones, 0.0f);
    int ret = bgsm_get_all_d2_output(system, d2_output.data());
    ASSERT_EQ(ret, 0);

    for (uint32_t i = 0; i < config.num_matrix_zones; i++) {
        EXPECT_GE(d2_output[i], 0.0f);
        EXPECT_LE(d2_output[i], 1.0f);
    }
}

//=============================================================================
// Interaction Tests
//=============================================================================

TEST_F(StriosomeMatrixTest, StriosomeModulatesMatrix) {
    // Process striosomes with high motivation
    std::vector<float> limbic_input(config.num_striosomes, 0.8f);
    bgsm_set_striosome_input(system, BGSM_INPUT_LIMBIC, limbic_input.data());
    bgsm_process_striosomes(system);

    // Process matrix
    std::vector<float> motor_input(config.num_matrix_zones, 0.5f);
    bgsm_set_matrix_input(system, BGSM_INPUT_MOTOR, motor_input.data());
    bgsm_set_matrix_dopamine(system, 0.5f);
    bgsm_process_matrix(system);

    // Apply striosome modulation to matrix
    int ret = bgsm_apply_striosome_modulation(system);
    ASSERT_EQ(ret, 0);
}

TEST_F(StriosomeMatrixTest, BoundaryInteraction) {
    // Set up both compartments
    std::vector<float> limbic_input(config.num_striosomes, 0.6f);
    bgsm_set_striosome_input(system, BGSM_INPUT_LIMBIC, limbic_input.data());

    std::vector<float> motor_input(config.num_matrix_zones, 0.5f);
    bgsm_set_matrix_input(system, BGSM_INPUT_MOTOR, motor_input.data());

    bgsm_process_striosomes(system);
    bgsm_process_matrix(system);

    // Process boundary spillover
    int ret = bgsm_process_boundary(system);
    ASSERT_EQ(ret, 0);
}

//=============================================================================
// Update/Step Tests
//=============================================================================

TEST_F(StriosomeMatrixTest, Step) {
    int ret = bgsm_step(system, 10.0f);
    ASSERT_EQ(ret, 0);
}

TEST_F(StriosomeMatrixTest, FullProcess) {
    // Set inputs
    std::vector<float> limbic_input(config.num_striosomes, 0.6f);
    bgsm_set_striosome_input(system, BGSM_INPUT_LIMBIC, limbic_input.data());

    std::vector<float> motor_input(config.num_matrix_zones, 0.5f);
    bgsm_set_matrix_input(system, BGSM_INPUT_MOTOR, motor_input.data());
    bgsm_set_matrix_dopamine(system, 0.5f);

    // Full processing cycle
    int ret = bgsm_process(system);
    ASSERT_EQ(ret, 0);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(StriosomeMatrixTest, GetStats) {
    // Process some data
    std::vector<float> limbic_input(config.num_striosomes, 0.6f);
    bgsm_set_striosome_input(system, BGSM_INPUT_LIMBIC, limbic_input.data());
    bgsm_process_striosomes(system);

    std::vector<float> motor_input(config.num_matrix_zones, 0.5f);
    bgsm_set_matrix_input(system, BGSM_INPUT_MOTOR, motor_input.data());
    bgsm_process_matrix(system);

    bgsm_stats_t stats;
    int ret = bgsm_get_stats(system, &stats);
    ASSERT_EQ(ret, 0);

    EXPECT_GE(stats.avg_striosome_activation, 0.0f);
    EXPECT_GE(stats.avg_matrix_activation, 0.0f);
}

//=============================================================================
// Utility Tests
//=============================================================================

TEST_F(StriosomeMatrixTest, CompartmentName) {
    EXPECT_STREQ(bgsm_compartment_name(BGSM_COMPARTMENT_STRIOSOME), "Striosome");
    EXPECT_STREQ(bgsm_compartment_name(BGSM_COMPARTMENT_MATRIX), "Matrix");
}

TEST_F(StriosomeMatrixTest, StateName) {
    EXPECT_STREQ(bgsm_state_name(BGSM_STATE_BASELINE), "Baseline");
    EXPECT_STREQ(bgsm_state_name(BGSM_STATE_ACTIVATED), "Activated");
    EXPECT_STREQ(bgsm_state_name(BGSM_STATE_SUPPRESSED), "Suppressed");
    EXPECT_STREQ(bgsm_state_name(BGSM_STATE_BURST), "Burst");
}

//=============================================================================
// Error Handling Tests
//=============================================================================

TEST_F(StriosomeMatrixTest, NullInputHandling) {
    EXPECT_NE(bgsm_set_striosome_input(nullptr, BGSM_INPUT_LIMBIC, nullptr), 0);
    EXPECT_NE(bgsm_set_matrix_input(nullptr, BGSM_INPUT_MOTOR, nullptr), 0);
    EXPECT_NE(bgsm_process_striosomes(nullptr), 0);
    EXPECT_NE(bgsm_process_matrix(nullptr), 0);
    EXPECT_NE(bgsm_step(nullptr, 10.0f), 0);
}

TEST_F(StriosomeMatrixTest, InvalidStriosomeId) {
    // Getting activation for invalid striosome should return 0 or handle gracefully
    float activation = bgsm_get_striosome_activation(system, 9999);
    EXPECT_GE(activation, 0.0f);  // Should return 0 for invalid
}

//=============================================================================
// Bidirectional Data Flow Tests
//=============================================================================

TEST_F(StriosomeMatrixTest, BidirectionalStriosomeToSNc) {
    // Input: Limbic cortex -> Striosome
    std::vector<float> limbic_input(config.num_striosomes, 0.8f);
    bgsm_set_striosome_input(system, BGSM_INPUT_LIMBIC, limbic_input.data());
    bgsm_process_striosomes(system);

    // Output: Striosome -> SNc (dopamine neurons)
    float snc_modulation = bgsm_get_snc_modulation(system);

    // Verify output is produced
    EXPECT_NE(snc_modulation, 0.0f);  // Should have some modulation

    // This SNc output can then be fed back to affect dopamine release
    // which in turn affects the matrix - completing the loop
}

TEST_F(StriosomeMatrixTest, BidirectionalMatrixThroughPathways) {
    // Input: Motor cortex -> Matrix
    std::vector<float> motor_input(config.num_matrix_zones, 0.6f);
    bgsm_set_matrix_input(system, BGSM_INPUT_MOTOR, motor_input.data());
    bgsm_set_matrix_dopamine(system, 0.6f);
    bgsm_process_matrix(system);

    // Output: Matrix -> GPi/GPe/SNr (through D1/D2 pathways)
    std::vector<float> d1_output(config.num_matrix_zones);
    std::vector<float> d2_output(config.num_matrix_zones);

    bgsm_get_all_d1_output(system, d1_output.data());
    bgsm_get_all_d2_output(system, d2_output.data());

    // Both pathways should be active
    float d1_sum = 0.0f, d2_sum = 0.0f;
    for (uint32_t i = 0; i < config.num_matrix_zones; i++) {
        d1_sum += d1_output[i];
        d2_sum += d2_output[i];
    }

    EXPECT_GT(d1_sum, 0.0f);
    EXPECT_GT(d2_sum, 0.0f);
}

TEST_F(StriosomeMatrixTest, CrossCompartmentModulation) {
    ASSERT_NE(system, nullptr);

    // Set up striosomes (motivation/value)
    std::vector<float> limbic_input(config.num_striosomes, 0.9f);
    bgsm_set_striosome_input(system, BGSM_INPUT_LIMBIC, limbic_input.data());
    bgsm_process_striosomes(system);

    // Get motivation signal from striosomes
    float motivation = bgsm_get_motivation(system);
    EXPECT_GT(motivation, 0.5f);  // High motivation

    // Set up matrix (action)
    std::vector<float> motor_input(config.num_matrix_zones, 0.5f);
    bgsm_set_matrix_input(system, BGSM_INPUT_MOTOR, motor_input.data());
    bgsm_process_matrix(system);

    // Get D1 output before modulation
    float d1_before = bgsm_get_d1_output(system, 0);

    // Apply striosome modulation to matrix
    bgsm_apply_striosome_modulation(system);

    // D1 output may change due to motivational gating
    // This demonstrates the bidirectional influence between compartments
}
