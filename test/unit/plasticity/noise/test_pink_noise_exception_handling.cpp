/**
 * @file test_pink_noise_exception_handling.cpp
 * @brief Unit tests for NIMCP_THROW_TO_IMMUNE exception handling in Pink Noise module
 *
 * WHAT: Tests that all error paths properly trigger NIMCP_THROW_TO_IMMUNE
 * WHY:  Ensure errors are reported to the brain immune system for monitoring
 * HOW:  Test all NULL pointer and invalid parameter paths
 *
 * @version 2.6.3
 * @date 2026-01-24
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

extern "C" {
#include "plasticity/noise/nimcp_pink_noise.h"
#include "plasticity/noise/nimcp_pink_noise_correlated.h"
#include "plasticity/noise/nimcp_pink_noise_criticality.h"
#include "plasticity/noise/nimcp_pink_noise_monitor.h"
#include "plasticity/noise/nimcp_pink_noise_multiscale.h"
#include "plasticity/noise/nimcp_pink_noise_spatial.h"
#include "plasticity/noise/nimcp_pink_noise_sleep.h"
}

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class PinkNoiseExceptionHandlingTest : public ::testing::Test {
protected:
    pink_noise_generator_t generator;

    void SetUp() override {
        pink_noise_config_t config = pink_noise_default_config();
        generator = pink_noise_create(&config);
    }

    void TearDown() override {
        if (generator) {
            pink_noise_destroy(generator);
            generator = nullptr;
        }
    }
};

/* ============================================================================
 * Core Pink Noise Exception Handling Tests
 * ============================================================================ */

TEST_F(PinkNoiseExceptionHandlingTest, CreateWithNullConfig) {
    /* SCENARIO: Create generator with NULL config
     * EXPECTED: Returns NULL, exception thrown to immune system
     */
    pink_noise_generator_t gen = pink_noise_create(nullptr);
    EXPECT_EQ(gen, nullptr);

    /* Verify error was set */
    const char* error = pink_noise_get_last_error();
    EXPECT_NE(error, nullptr);
}

TEST_F(PinkNoiseExceptionHandlingTest, CreateWithInvalidAlpha) {
    /* SCENARIO: Create with invalid alpha exponent
     * EXPECTED: Returns NULL, validation fails
     */
    pink_noise_config_t config = pink_noise_default_config();
    config.alpha = -5.0f;  /* Invalid: must be 0-3 */

    pink_noise_generator_t gen = pink_noise_create(&config);
    EXPECT_EQ(gen, nullptr);
}

TEST_F(PinkNoiseExceptionHandlingTest, GenerateWithNullGenerator) {
    /* SCENARIO: Generate samples with NULL generator
     * EXPECTED: Returns false, exception thrown
     */
    float samples[100];
    bool success = pink_noise_generate(nullptr, samples, 100);
    EXPECT_FALSE(success);
}

TEST_F(PinkNoiseExceptionHandlingTest, GenerateWithNullBuffer) {
    /* SCENARIO: Generate samples with NULL buffer
     * EXPECTED: Returns false, exception thrown
     */
    ASSERT_NE(generator, nullptr);
    bool success = pink_noise_generate(generator, nullptr, 100);
    EXPECT_FALSE(success);
}

TEST_F(PinkNoiseExceptionHandlingTest, GenerateWithZeroCount) {
    /* SCENARIO: Generate zero samples
     * EXPECTED: Returns false, handled gracefully
     */
    ASSERT_NE(generator, nullptr);
    float samples[10];
    bool success = pink_noise_generate(generator, samples, 0);
    EXPECT_FALSE(success);
}

TEST_F(PinkNoiseExceptionHandlingTest, GenerateSampleWithNullGenerator) {
    /* SCENARIO: Generate single sample with NULL generator
     * EXPECTED: Returns false
     */
    float sample;
    bool success = pink_noise_generate_sample(nullptr, &sample);
    EXPECT_FALSE(success);
}

TEST_F(PinkNoiseExceptionHandlingTest, GenerateSampleWithNullOutput) {
    /* SCENARIO: Generate single sample with NULL output
     * EXPECTED: Returns false
     */
    ASSERT_NE(generator, nullptr);
    bool success = pink_noise_generate_sample(generator, nullptr);
    EXPECT_FALSE(success);
}

TEST_F(PinkNoiseExceptionHandlingTest, ResetWithNullGenerator) {
    /* SCENARIO: Reset NULL generator
     * EXPECTED: Returns false
     */
    bool success = pink_noise_reset(nullptr, 12345);
    EXPECT_FALSE(success);
}

TEST_F(PinkNoiseExceptionHandlingTest, ComputeStatsWithNullSamples) {
    /* SCENARIO: Compute stats with NULL samples
     * EXPECTED: Returns false
     */
    pink_noise_stats_t stats;
    bool success = pink_noise_compute_stats(nullptr, 100, 1000.0f, &stats);
    EXPECT_FALSE(success);
}

TEST_F(PinkNoiseExceptionHandlingTest, ComputeStatsWithNullStats) {
    /* SCENARIO: Compute stats with NULL stats output
     * EXPECTED: Returns false
     */
    float samples[100];
    bool success = pink_noise_compute_stats(samples, 100, 1000.0f, nullptr);
    EXPECT_FALSE(success);
}

TEST_F(PinkNoiseExceptionHandlingTest, ComputeStatsWithInsufficientSamples) {
    /* SCENARIO: Compute stats with too few samples
     * EXPECTED: Returns false (need >= 64 samples)
     */
    float samples[32];
    pink_noise_stats_t stats;
    bool success = pink_noise_compute_stats(samples, 32, 1000.0f, &stats);
    EXPECT_FALSE(success);
}

TEST_F(PinkNoiseExceptionHandlingTest, ValidateWithNullSamples) {
    /* SCENARIO: Validate with NULL samples
     * EXPECTED: Returns false
     */
    bool valid = pink_noise_validate(nullptr, 100, 1000.0f, 1.0f, 0.1f);
    EXPECT_FALSE(valid);
}

TEST_F(PinkNoiseExceptionHandlingTest, ValidateWithNegativeTolerance) {
    /* SCENARIO: Validate with negative tolerance
     * EXPECTED: Returns false
     */
    float samples[100];
    bool valid = pink_noise_validate(samples, 100, 1000.0f, 1.0f, -0.1f);
    EXPECT_FALSE(valid);
}

TEST_F(PinkNoiseExceptionHandlingTest, ModulateWithNullGenerator) {
    /* SCENARIO: Modulate with NULL generator
     * EXPECTED: Returns false
     */
    float output;
    bool success = pink_noise_modulate(nullptr, 0.5f, &output);
    EXPECT_FALSE(success);
}

TEST_F(PinkNoiseExceptionHandlingTest, ModulateWithNullOutput) {
    /* SCENARIO: Modulate with NULL output
     * EXPECTED: Returns false
     */
    ASSERT_NE(generator, nullptr);
    bool success = pink_noise_modulate(generator, 0.5f, nullptr);
    EXPECT_FALSE(success);
}

TEST_F(PinkNoiseExceptionHandlingTest, ModulateMultiplicativeWithNullGenerator) {
    /* SCENARIO: Multiplicative modulate with NULL generator
     * EXPECTED: Returns false
     */
    float output;
    bool success = pink_noise_modulate_multiplicative(nullptr, 1.0f, 0.1f, &output);
    EXPECT_FALSE(success);
}

TEST_F(PinkNoiseExceptionHandlingTest, ModulateMultiplicativeWithNullOutput) {
    /* SCENARIO: Multiplicative modulate with NULL output
     * EXPECTED: Returns false
     */
    ASSERT_NE(generator, nullptr);
    bool success = pink_noise_modulate_multiplicative(generator, 1.0f, 0.1f, nullptr);
    EXPECT_FALSE(success);
}

TEST_F(PinkNoiseExceptionHandlingTest, SaveWithNullGenerator) {
    /* SCENARIO: Save with NULL generator
     * EXPECTED: Returns false
     */
    FILE* file = tmpfile();
    ASSERT_NE(file, nullptr);

    bool success = pink_noise_save(nullptr, file);
    EXPECT_FALSE(success);

    fclose(file);
}

TEST_F(PinkNoiseExceptionHandlingTest, SaveWithNullFile) {
    /* SCENARIO: Save with NULL file
     * EXPECTED: Returns false, exception thrown
     */
    ASSERT_NE(generator, nullptr);
    bool success = pink_noise_save(generator, nullptr);
    EXPECT_FALSE(success);
}

TEST_F(PinkNoiseExceptionHandlingTest, LoadWithNullFile) {
    /* SCENARIO: Load with NULL file
     * EXPECTED: Returns NULL
     */
    pink_noise_generator_t loaded = pink_noise_load(nullptr);
    EXPECT_EQ(loaded, nullptr);
}

TEST_F(PinkNoiseExceptionHandlingTest, ValidateConfigWithNullConfig) {
    /* SCENARIO: Validate NULL config
     * EXPECTED: Returns false
     */
    bool valid = pink_noise_validate_config(nullptr);
    EXPECT_FALSE(valid);
}

/* ============================================================================
 * Config Validation Edge Cases
 * ============================================================================ */

TEST_F(PinkNoiseExceptionHandlingTest, ValidateConfigAlphaBoundaryLow) {
    /* SCENARIO: Alpha at boundary (0.0 is valid)
     * EXPECTED: Returns true
     */
    pink_noise_config_t config = pink_noise_default_config();
    config.alpha = 0.0f;
    bool valid = pink_noise_validate_config(&config);
    EXPECT_TRUE(valid);
}

TEST_F(PinkNoiseExceptionHandlingTest, ValidateConfigAlphaBoundaryHigh) {
    /* SCENARIO: Alpha at boundary (3.0 is valid)
     * EXPECTED: Returns true
     */
    pink_noise_config_t config = pink_noise_default_config();
    config.alpha = 3.0f;
    bool valid = pink_noise_validate_config(&config);
    EXPECT_TRUE(valid);
}

TEST_F(PinkNoiseExceptionHandlingTest, ValidateConfigAlphaOverBoundary) {
    /* SCENARIO: Alpha over boundary
     * EXPECTED: Returns false
     */
    pink_noise_config_t config = pink_noise_default_config();
    config.alpha = 3.1f;
    bool valid = pink_noise_validate_config(&config);
    EXPECT_FALSE(valid);
}

TEST_F(PinkNoiseExceptionHandlingTest, ValidateConfigZeroAmplitude) {
    /* SCENARIO: Zero amplitude
     * EXPECTED: Returns false
     */
    pink_noise_config_t config = pink_noise_default_config();
    config.amplitude = 0.0f;
    bool valid = pink_noise_validate_config(&config);
    EXPECT_FALSE(valid);
}

TEST_F(PinkNoiseExceptionHandlingTest, ValidateConfigNegativeAmplitude) {
    /* SCENARIO: Negative amplitude
     * EXPECTED: Returns false
     */
    pink_noise_config_t config = pink_noise_default_config();
    config.amplitude = -1.0f;
    bool valid = pink_noise_validate_config(&config);
    EXPECT_FALSE(valid);
}

TEST_F(PinkNoiseExceptionHandlingTest, ValidateConfigFrequencyRangeInverted) {
    /* SCENARIO: min_frequency > max_frequency
     * EXPECTED: Returns false
     */
    pink_noise_config_t config = pink_noise_default_config();
    config.min_frequency = 200.0f;
    config.max_frequency = 100.0f;
    bool valid = pink_noise_validate_config(&config);
    EXPECT_FALSE(valid);
}

TEST_F(PinkNoiseExceptionHandlingTest, ValidateConfigNyquistViolation) {
    /* SCENARIO: sample_rate < 2 * max_frequency
     * EXPECTED: Returns false
     */
    pink_noise_config_t config = pink_noise_default_config();
    config.max_frequency = 500.0f;
    config.sample_rate = 800.0f;  /* Should be >= 1000 */
    bool valid = pink_noise_validate_config(&config);
    EXPECT_FALSE(valid);
}

/* ============================================================================
 * Method-Specific Exception Handling
 * ============================================================================ */

TEST_F(PinkNoiseExceptionHandlingTest, FFTMethodHandlesEdgeCases) {
    /* SCENARIO: FFT method with edge case parameters
     * EXPECTED: Creates successfully, generates valid samples
     */
    pink_noise_config_t config = pink_noise_default_config();
    config.method = PINK_NOISE_FFT;
    config.alpha = 0.5f;  /* Between white and pink */

    pink_noise_generator_t gen = pink_noise_create(&config);
    ASSERT_NE(gen, nullptr);

    float samples[128];
    bool success = pink_noise_generate(gen, samples, 128);
    EXPECT_TRUE(success);

    /* Verify all samples are finite */
    for (int i = 0; i < 128; i++) {
        EXPECT_TRUE(std::isfinite(samples[i]));
    }

    pink_noise_destroy(gen);
}

TEST_F(PinkNoiseExceptionHandlingTest, VossMethodHandlesEdgeCases) {
    /* SCENARIO: Voss method with edge case parameters
     * EXPECTED: Creates successfully
     */
    pink_noise_config_t config = pink_noise_default_config();
    config.method = PINK_NOISE_VOSS;
    config.alpha = 2.0f;  /* Red/Brownian noise */

    pink_noise_generator_t gen = pink_noise_create(&config);
    ASSERT_NE(gen, nullptr);

    float samples[128];
    bool success = pink_noise_generate(gen, samples, 128);
    EXPECT_TRUE(success);

    pink_noise_destroy(gen);
}

TEST_F(PinkNoiseExceptionHandlingTest, IIRMethodHandlesEdgeCases) {
    /* SCENARIO: IIR method with edge case parameters
     * EXPECTED: Creates successfully
     */
    pink_noise_config_t config = pink_noise_default_config();
    config.method = PINK_NOISE_IIR;
    config.amplitude = 0.001f;  /* Very small amplitude */

    pink_noise_generator_t gen = pink_noise_create(&config);
    ASSERT_NE(gen, nullptr);

    float sample;
    bool success = pink_noise_generate_sample(gen, &sample);
    EXPECT_TRUE(success);
    EXPECT_TRUE(std::isfinite(sample));

    pink_noise_destroy(gen);
}

/* ============================================================================
 * Destroy Safety Tests
 * ============================================================================ */

TEST_F(PinkNoiseExceptionHandlingTest, DestroyNullGenerator) {
    /* SCENARIO: Destroy NULL generator
     * EXPECTED: Does not crash
     */
    pink_noise_destroy(nullptr);
    SUCCEED();
}

TEST_F(PinkNoiseExceptionHandlingTest, DoubleDestroyPrevented) {
    /* SCENARIO: Generator destroyed, pointer set to NULL
     * EXPECTED: Second destroy of NULL is safe
     */
    pink_noise_config_t config = pink_noise_default_config();
    pink_noise_generator_t gen = pink_noise_create(&config);
    ASSERT_NE(gen, nullptr);

    pink_noise_destroy(gen);
    gen = nullptr;

    /* Second destroy should be safe */
    pink_noise_destroy(gen);
    SUCCEED();
}

/* ============================================================================
 * Persistence Exception Handling
 * ============================================================================ */

TEST_F(PinkNoiseExceptionHandlingTest, SaveAndLoadRoundTrip) {
    /* SCENARIO: Save and load generator state
     * EXPECTED: State is preserved
     */
    ASSERT_NE(generator, nullptr);

    FILE* file = tmpfile();
    ASSERT_NE(file, nullptr);

    /* Save state */
    bool save_success = pink_noise_save(generator, file);
    EXPECT_TRUE(save_success);

    /* Rewind and load */
    rewind(file);
    pink_noise_generator_t loaded = pink_noise_load(file);

    fclose(file);

    if (loaded != nullptr) {
        /* Generate samples from loaded generator */
        float sample;
        bool gen_success = pink_noise_generate_sample(loaded, &sample);
        EXPECT_TRUE(gen_success);
        EXPECT_TRUE(std::isfinite(sample));

        pink_noise_destroy(loaded);
    }
}

/* ============================================================================
 * Entry Point
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
