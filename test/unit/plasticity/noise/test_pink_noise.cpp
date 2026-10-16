/**
 * @file test_pink_noise.cpp
 * @brief Comprehensive unit tests for Pink Noise module (100% coverage)
 *
 * WHAT: Unit tests covering all pink noise functionality
 * WHY:  Ensure 100% code coverage and correctness of implementation
 * HOW:  Test all functions, methods, edge cases, and error paths
 *
 * TEST COVERAGE TARGETS:
 * - Config validation (all parameters, all error paths)
 * - Generator lifecycle (create, destroy, reset)
 * - All generation methods (FFT, Voss, IIR, White)
 * - Sample generation (single, batch)
 * - Spectral analysis (DFT, regression, R²)
 * - Modulation functions (additive, multiplicative)
 * - Error handling (NULL pointers, invalid params)
 * - Edge cases (zero samples, buffer wraparound, etc.)
 *
 * @version Unit Testing Framework v1.0
 * @date 2025-11-13
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>

    #include "plasticity/noise/nimcp_pink_noise.h"

//=============================================================================
// Test Fixture
//=============================================================================

class PinkNoiseUnitTest : public ::testing::Test {
protected:
    pink_noise_generator_t generator;

    void SetUp() override {
        generator = nullptr;
    }

    void TearDown() override {
        if (generator) {
            pink_noise_destroy(generator);
            generator = nullptr;
        }
    }
};

//=============================================================================
// Config Validation Tests
//=============================================================================

TEST_F(PinkNoiseUnitTest, ConfigValidation_NullConfig) {
    // WHAT: Test NULL config handling
    bool valid = pink_noise_validate_config(nullptr);
    EXPECT_FALSE(valid);
}

TEST_F(PinkNoiseUnitTest, ConfigValidation_InvalidAlpha_TooLow) {
    pink_noise_config_t config = pink_noise_default_config();
    config.alpha = -0.1f;
    bool valid = pink_noise_validate_config(&config);
    EXPECT_FALSE(valid);
}

TEST_F(PinkNoiseUnitTest, ConfigValidation_InvalidAlpha_TooHigh) {
    pink_noise_config_t config = pink_noise_default_config();
    config.alpha = 3.5f;
    bool valid = pink_noise_validate_config(&config);
    EXPECT_FALSE(valid);
}

TEST_F(PinkNoiseUnitTest, ConfigValidation_InvalidAmplitude_Zero) {
    pink_noise_config_t config = pink_noise_default_config();
    config.amplitude = 0.0f;
    bool valid = pink_noise_validate_config(&config);
    EXPECT_FALSE(valid);
}

TEST_F(PinkNoiseUnitTest, ConfigValidation_InvalidAmplitude_Negative) {
    pink_noise_config_t config = pink_noise_default_config();
    config.amplitude = -0.1f;
    bool valid = pink_noise_validate_config(&config);
    EXPECT_FALSE(valid);
}

TEST_F(PinkNoiseUnitTest, ConfigValidation_InvalidFrequencyRange) {
    pink_noise_config_t config = pink_noise_default_config();
    config.min_frequency = 100.0f;
    config.max_frequency = 10.0f;  // min > max
    bool valid = pink_noise_validate_config(&config);
    EXPECT_FALSE(valid);
}

TEST_F(PinkNoiseUnitTest, ConfigValidation_NyquistViolation) {
    pink_noise_config_t config = pink_noise_default_config();
    config.max_frequency = 500.0f;
    config.sample_rate = 800.0f;  // < 2 * max_freq
    bool valid = pink_noise_validate_config(&config);
    EXPECT_FALSE(valid);
}

TEST_F(PinkNoiseUnitTest, ConfigValidation_ValidConfig) {
    pink_noise_config_t config = pink_noise_default_config();
    bool valid = pink_noise_validate_config(&config);
    EXPECT_TRUE(valid);
}

//=============================================================================
// Generator Lifecycle Tests
//=============================================================================

TEST_F(PinkNoiseUnitTest, GeneratorCreate_NullConfig) {
    generator = pink_noise_create(nullptr);
    EXPECT_EQ(generator, nullptr);
}

TEST_F(PinkNoiseUnitTest, GeneratorCreate_InvalidConfig) {
    pink_noise_config_t config = pink_noise_default_config();
    config.alpha = -1.0f;  // Invalid
    generator = pink_noise_create(&config);
    EXPECT_EQ(generator, nullptr);
}

TEST_F(PinkNoiseUnitTest, GeneratorCreate_ValidConfig_Voss) {
    pink_noise_config_t config = pink_noise_default_config();
    config.method = PINK_NOISE_VOSS;
    generator = pink_noise_create(&config);
    ASSERT_NE(generator, nullptr);
}

TEST_F(PinkNoiseUnitTest, GeneratorCreate_ValidConfig_IIR) {
    pink_noise_config_t config = pink_noise_default_config();
    config.method = PINK_NOISE_IIR;
    generator = pink_noise_create(&config);
    ASSERT_NE(generator, nullptr);
}

TEST_F(PinkNoiseUnitTest, GeneratorCreate_ValidConfig_FFT) {
    pink_noise_config_t config = pink_noise_default_config();
    config.method = PINK_NOISE_FFT;
    generator = pink_noise_create(&config);
    ASSERT_NE(generator, nullptr);
}

TEST_F(PinkNoiseUnitTest, GeneratorCreate_ValidConfig_White) {
    pink_noise_config_t config = pink_noise_default_config();
    config.method = PINK_NOISE_WHITE;
    generator = pink_noise_create(&config);
    ASSERT_NE(generator, nullptr);
}

TEST_F(PinkNoiseUnitTest, GeneratorCreate_SeedHandling) {
    pink_noise_config_t config = pink_noise_default_config();
    config.seed = 12345;
    generator = pink_noise_create(&config);
    ASSERT_NE(generator, nullptr);
}

TEST_F(PinkNoiseUnitTest, GeneratorDestroy_NullGenerator) {
    // Should not crash
    pink_noise_destroy(nullptr);
    SUCCEED();
}

TEST_F(PinkNoiseUnitTest, GeneratorDestroy_ValidGenerator) {
    pink_noise_config_t config = pink_noise_default_config();
    generator = pink_noise_create(&config);
    ASSERT_NE(generator, nullptr);
    pink_noise_destroy(generator);
    generator = nullptr;  // Prevent double-free in TearDown
    SUCCEED();
}

//=============================================================================
// Sample Generation Tests - Single Sample
//=============================================================================

TEST_F(PinkNoiseUnitTest, GenerateSample_NullGenerator) {
    float sample;
    bool success = pink_noise_generate_sample(nullptr, &sample);
    EXPECT_FALSE(success);
}

TEST_F(PinkNoiseUnitTest, GenerateSample_NullSamplePointer) {
    pink_noise_config_t config = pink_noise_default_config();
    generator = pink_noise_create(&config);
    ASSERT_NE(generator, nullptr);

    bool success = pink_noise_generate_sample(generator, nullptr);
    EXPECT_FALSE(success);
}

TEST_F(PinkNoiseUnitTest, GenerateSample_Voss_Success) {
    pink_noise_config_t config = pink_noise_default_config();
    config.method = PINK_NOISE_VOSS;
    generator = pink_noise_create(&config);
    ASSERT_NE(generator, nullptr);

    float sample;
    bool success = pink_noise_generate_sample(generator, &sample);
    EXPECT_TRUE(success);
    EXPECT_TRUE(std::isfinite(sample));
}

TEST_F(PinkNoiseUnitTest, GenerateSample_IIR_Success) {
    pink_noise_config_t config = pink_noise_default_config();
    config.method = PINK_NOISE_IIR;
    generator = pink_noise_create(&config);
    ASSERT_NE(generator, nullptr);

    float sample;
    bool success = pink_noise_generate_sample(generator, &sample);
    EXPECT_TRUE(success);
    EXPECT_TRUE(std::isfinite(sample));
}

TEST_F(PinkNoiseUnitTest, GenerateSample_FFT_Success) {
    pink_noise_config_t config = pink_noise_default_config();
    config.method = PINK_NOISE_FFT;
    generator = pink_noise_create(&config);
    ASSERT_NE(generator, nullptr);

    float sample;
    bool success = pink_noise_generate_sample(generator, &sample);
    EXPECT_TRUE(success);
    EXPECT_TRUE(std::isfinite(sample));
}

TEST_F(PinkNoiseUnitTest, GenerateSample_White_Success) {
    pink_noise_config_t config = pink_noise_default_config();
    config.method = PINK_NOISE_WHITE;
    generator = pink_noise_create(&config);
    ASSERT_NE(generator, nullptr);

    float sample;
    bool success = pink_noise_generate_sample(generator, &sample);
    EXPECT_TRUE(success);
    EXPECT_TRUE(std::isfinite(sample));
}

TEST_F(PinkNoiseUnitTest, GenerateSample_MultipleSequential) {
    pink_noise_config_t config = pink_noise_default_config();
    config.method = PINK_NOISE_VOSS;
    generator = pink_noise_create(&config);
    ASSERT_NE(generator, nullptr);

    // Generate 100 samples
    for (int i = 0; i < 100; ++i) {
        float sample;
        bool success = pink_noise_generate_sample(generator, &sample);
        ASSERT_TRUE(success);
        EXPECT_TRUE(std::isfinite(sample));
    }
}

//=============================================================================
// Sample Generation Tests - Batch
//=============================================================================

TEST_F(PinkNoiseUnitTest, GenerateBatch_NullGenerator) {
    float samples[100];
    bool success = pink_noise_generate(nullptr, samples, 100);
    EXPECT_FALSE(success);
}

TEST_F(PinkNoiseUnitTest, GenerateBatch_NullSamplesArray) {
    pink_noise_config_t config = pink_noise_default_config();
    generator = pink_noise_create(&config);
    ASSERT_NE(generator, nullptr);

    bool success = pink_noise_generate(generator, nullptr, 100);
    EXPECT_FALSE(success);
}

TEST_F(PinkNoiseUnitTest, GenerateBatch_ZeroSamples) {
    pink_noise_config_t config = pink_noise_default_config();
    generator = pink_noise_create(&config);
    ASSERT_NE(generator, nullptr);

    float samples[10];
    bool success = pink_noise_generate(generator, samples, 0);
    EXPECT_FALSE(success);
}

TEST_F(PinkNoiseUnitTest, GenerateBatch_Voss_Success) {
    pink_noise_config_t config = pink_noise_default_config();
    config.method = PINK_NOISE_VOSS;
    generator = pink_noise_create(&config);
    ASSERT_NE(generator, nullptr);

    float samples[256];
    bool success = pink_noise_generate(generator, samples, 256);
    ASSERT_TRUE(success);

    for (int i = 0; i < 256; ++i) {
        EXPECT_TRUE(std::isfinite(samples[i]));
    }
}

TEST_F(PinkNoiseUnitTest, GenerateBatch_FFT_BufferWraparound) {
    // WHAT: Test FFT buffer wraparound after consuming all samples
    pink_noise_config_t config = pink_noise_default_config();
    config.method = PINK_NOISE_FFT;
    generator = pink_noise_create(&config);
    ASSERT_NE(generator, nullptr);

    // Generate enough samples to wrap around buffer multiple times
    float samples[2048];
    bool success = pink_noise_generate(generator, samples, 2048);
    ASSERT_TRUE(success);

    for (int i = 0; i < 2048; ++i) {
        EXPECT_TRUE(std::isfinite(samples[i]));
    }
}

//=============================================================================
// Reset Functionality Tests
//=============================================================================

TEST_F(PinkNoiseUnitTest, Reset_NullGenerator) {
    bool success = pink_noise_reset(nullptr, 12345);
    EXPECT_FALSE(success);
}

TEST_F(PinkNoiseUnitTest, Reset_Reproducibility_Voss) {
    pink_noise_config_t config = pink_noise_default_config();
    config.method = PINK_NOISE_VOSS;
    config.seed = 42;
    generator = pink_noise_create(&config);
    ASSERT_NE(generator, nullptr);

    float seq1[50];
    pink_noise_generate(generator, seq1, 50);

    pink_noise_reset(generator, 42);
    float seq2[50];
    pink_noise_generate(generator, seq2, 50);

    for (int i = 0; i < 50; ++i) {
        EXPECT_FLOAT_EQ(seq1[i], seq2[i]) << "Same seed should reproduce sequence";
    }
}

TEST_F(PinkNoiseUnitTest, Reset_FFT_BufferPositionReset) {
    pink_noise_config_t config = pink_noise_default_config();
    config.method = PINK_NOISE_FFT;
    config.seed = 123;
    generator = pink_noise_create(&config);
    ASSERT_NE(generator, nullptr);

    // Generate some samples
    float samples1[10];
    pink_noise_generate(generator, samples1, 10);

    // Reset and generate again
    pink_noise_reset(generator, 123);
    float samples2[10];
    pink_noise_generate(generator, samples2, 10);

    // Should get same sequence
    for (int i = 0; i < 10; ++i) {
        EXPECT_FLOAT_EQ(samples1[i], samples2[i]);
    }
}

//=============================================================================
// Statistics Computation Tests
//=============================================================================

TEST_F(PinkNoiseUnitTest, ComputeStats_NullSamples) {
    pink_noise_stats_t stats;
    bool success = pink_noise_compute_stats(nullptr, 100, 1000.0f, &stats);
    EXPECT_FALSE(success);
}

TEST_F(PinkNoiseUnitTest, ComputeStats_NullStats) {
    float samples[100];
    bool success = pink_noise_compute_stats(samples, 100, 1000.0f, nullptr);
    EXPECT_FALSE(success);
}

TEST_F(PinkNoiseUnitTest, ComputeStats_TooFewSamples) {
    float samples[32];
    pink_noise_stats_t stats;
    bool success = pink_noise_compute_stats(samples, 32, 1000.0f, &stats);
    EXPECT_FALSE(success);
}

TEST_F(PinkNoiseUnitTest, ComputeStats_BasicStatistics) {
    pink_noise_config_t config = pink_noise_default_config();
    config.amplitude = 0.1f;
    generator = pink_noise_create(&config);
    ASSERT_NE(generator, nullptr);

    float samples[512];
    pink_noise_generate(generator, samples, 512);

    pink_noise_stats_t stats;
    bool success = pink_noise_compute_stats(samples, 512, config.sample_rate, &stats);
    ASSERT_TRUE(success);

    // Verify basic statistics
    EXPECT_NEAR(stats.mean, 0.0f, 0.1f);  // Mean should be near zero
    EXPECT_GT(stats.std_dev, 0.0f);  // Non-zero variance
    EXPECT_LT(stats.min_value, stats.max_value);  // Valid range
}

TEST_F(PinkNoiseUnitTest, ComputeStats_SpectralAnalysis) {
    pink_noise_config_t config = pink_noise_default_config();
    config.method = PINK_NOISE_FFT;
    config.alpha = 1.0f;
    generator = pink_noise_create(&config);
    ASSERT_NE(generator, nullptr);

    float samples[1024];
    pink_noise_generate(generator, samples, 1024);

    pink_noise_stats_t stats;
    bool success = pink_noise_compute_stats(samples, 1024, config.sample_rate, &stats);
    ASSERT_TRUE(success);

    // Spectral analysis should run
    EXPECT_GT(stats.measured_alpha, 0.0f);
    EXPECT_GE(stats.spectral_fit_r2, 0.0f);
    EXPECT_LE(stats.spectral_fit_r2, 1.0f);
}

//=============================================================================
// Validation Tests
//=============================================================================

TEST_F(PinkNoiseUnitTest, Validate_InvalidTolerance) {
    float samples[100];
    bool valid = pink_noise_validate(samples, 100, 1000.0f, 1.0f, -0.1f);
    EXPECT_FALSE(valid);
}

TEST_F(PinkNoiseUnitTest, Validate_PinkNoise) {
    pink_noise_config_t config = pink_noise_default_config();
    config.method = PINK_NOISE_FFT;
    config.alpha = 1.0f;
    config.seed = 42;  // Fixed seed for reproducibility
    generator = pink_noise_create(&config);
    ASSERT_NE(generator, nullptr);

    float samples[1024];
    pink_noise_generate(generator, samples, 1024);

    // Relaxed validation (DFT on short sequences has variance)
    bool valid = pink_noise_validate(samples, 1024, config.sample_rate, 1.0f, 0.7f);
    EXPECT_TRUE(valid);
}

//=============================================================================
// Modulation Tests
//=============================================================================

TEST_F(PinkNoiseUnitTest, Modulate_NullGenerator) {
    float output;
    bool success = pink_noise_modulate(nullptr, 0.5f, &output);
    EXPECT_FALSE(success);
}

TEST_F(PinkNoiseUnitTest, Modulate_NullOutput) {
    pink_noise_config_t config = pink_noise_default_config();
    generator = pink_noise_create(&config);
    ASSERT_NE(generator, nullptr);

    bool success = pink_noise_modulate(generator, 0.5f, nullptr);
    EXPECT_FALSE(success);
}

TEST_F(PinkNoiseUnitTest, Modulate_AdditiveModulation) {
    pink_noise_config_t config = pink_noise_default_config();
    config.amplitude = 0.05f;
    generator = pink_noise_create(&config);
    ASSERT_NE(generator, nullptr);

    float base = 0.5f;
    float output;
    bool success = pink_noise_modulate(generator, base, &output);
    ASSERT_TRUE(success);
    EXPECT_TRUE(std::isfinite(output));

    // Should be near base ± amplitude
    EXPECT_GT(output, base - 0.5f);
    EXPECT_LT(output, base + 0.5f);
}

TEST_F(PinkNoiseUnitTest, ModulateMultiplicative_NullGenerator) {
    float output;
    bool success = pink_noise_modulate_multiplicative(nullptr, 1.0f, 0.1f, &output);
    EXPECT_FALSE(success);
}

TEST_F(PinkNoiseUnitTest, ModulateMultiplicative_NullOutput) {
    pink_noise_config_t config = pink_noise_default_config();
    generator = pink_noise_create(&config);
    ASSERT_NE(generator, nullptr);

    bool success = pink_noise_modulate_multiplicative(generator, 1.0f, 0.1f, nullptr);
    EXPECT_FALSE(success);
}

TEST_F(PinkNoiseUnitTest, ModulateMultiplicative_Success) {
    pink_noise_config_t config = pink_noise_default_config();
    generator = pink_noise_create(&config);
    ASSERT_NE(generator, nullptr);

    float value = 1.0f;
    float strength = 0.1f;
    float output;
    bool success = pink_noise_modulate_multiplicative(generator, value, strength, &output);
    ASSERT_TRUE(success);
    EXPECT_TRUE(std::isfinite(output));
}

//=============================================================================
// Utility Function Tests
//=============================================================================

TEST_F(PinkNoiseUnitTest, DefaultConfig_Values) {
    pink_noise_config_t config = pink_noise_default_config();

    EXPECT_EQ(config.alpha, 1.0f);
    EXPECT_GT(config.amplitude, 0.0f);
    EXPECT_LT(config.min_frequency, config.max_frequency);
    EXPECT_GE(config.sample_rate, 2.0f * config.max_frequency);
    EXPECT_EQ(config.seed, 0);
}

TEST_F(PinkNoiseUnitTest, MethodName_AllMethods) {
    EXPECT_STREQ(pink_noise_method_name(PINK_NOISE_FFT), "FFT");
    EXPECT_STREQ(pink_noise_method_name(PINK_NOISE_VOSS), "Voss");
    EXPECT_STREQ(pink_noise_method_name(PINK_NOISE_IIR), "IIR");
    EXPECT_STREQ(pink_noise_method_name(PINK_NOISE_WHITE), "White");
}

TEST_F(PinkNoiseUnitTest, GetLastError_AfterError) {
    // Trigger an error
    pink_noise_create(nullptr);

    const char* error = pink_noise_get_last_error();
    EXPECT_NE(error, nullptr);
    EXPECT_GT(strlen(error), 0);
}

//=============================================================================
// Edge Case Tests
//=============================================================================

TEST_F(PinkNoiseUnitTest, EdgeCase_AlphaZero_WhiteNoise) {
    pink_noise_config_t config = pink_noise_default_config();
    config.alpha = 0.0f;  // White noise
    config.method = PINK_NOISE_FFT;
    generator = pink_noise_create(&config);
    ASSERT_NE(generator, nullptr);

    float samples[256];
    bool success = pink_noise_generate(generator, samples, 256);
    EXPECT_TRUE(success);
}

TEST_F(PinkNoiseUnitTest, EdgeCase_AlphaTwo_BrownianNoise) {
    pink_noise_config_t config = pink_noise_default_config();
    config.alpha = 2.0f;  // Brownian/red noise
    config.method = PINK_NOISE_FFT;
    generator = pink_noise_create(&config);
    ASSERT_NE(generator, nullptr);

    float samples[256];
    bool success = pink_noise_generate(generator, samples, 256);
    EXPECT_TRUE(success);
}

TEST_F(PinkNoiseUnitTest, EdgeCase_VeryLargeAmplitude) {
    pink_noise_config_t config = pink_noise_default_config();
    config.amplitude = 10.0f;
    generator = pink_noise_create(&config);
    ASSERT_NE(generator, nullptr);

    float sample;
    bool success = pink_noise_generate_sample(generator, &sample);
    EXPECT_TRUE(success);
    EXPECT_TRUE(std::isfinite(sample));
}

TEST_F(PinkNoiseUnitTest, EdgeCase_VerySmallAmplitude) {
    pink_noise_config_t config = pink_noise_default_config();
    config.amplitude = 0.0001f;
    generator = pink_noise_create(&config);
    ASSERT_NE(generator, nullptr);

    float sample;
    bool success = pink_noise_generate_sample(generator, &sample);
    EXPECT_TRUE(success);
    EXPECT_TRUE(std::isfinite(sample));
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
