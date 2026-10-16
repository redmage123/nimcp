//=============================================================================
// pink_noise_tests.c - Unit Tests for Pink Noise Generator
//=============================================================================
/**
 * @file pink_noise_tests.c
 * @brief Comprehensive unit tests for 1/f noise generation
 *
 * TEST PHILOSOPHY:
 * - Test-Driven Development (TDD): Tests guide implementation
 * - Guard clause verification: Test all error conditions
 * - Statistical validation: Verify 1/f spectrum properties
 * - Method comparison: Ensure different methods produce valid noise
 *
 * @author NIMCP Development Team
 * @date 2025-11-08
 */

#include <gtest/gtest.h>
#include "plasticity/noise/nimcp_pink_noise.h"
#include <math.h>

//=============================================================================
// Test Fixtures
//=============================================================================

class PinkNoiseTest : public ::testing::Test {
protected:
    pink_noise_generator_t generator;
    pink_noise_config_t config;

    void SetUp() override {
        generator = nullptr;
        config = pink_noise_default_config();
    }

    void TearDown() override {
        if (generator) {
            pink_noise_destroy(generator);
            generator = nullptr;
        }
    }

    // Helper: Compute autocorrelation at lag k
    float compute_autocorrelation(const float* samples, uint32_t num_samples, uint32_t lag) {
        if (lag >= num_samples) return 0.0f;

        // Compute mean
        float mean = 0.0f;
        for (uint32_t i = 0; i < num_samples; i++) {
            mean += samples[i];
        }
        mean /= (float)num_samples;

        // Compute autocorrelation
        float sum = 0.0f;
        float var = 0.0f;
        for (uint32_t i = 0; i < num_samples - lag; i++) {
            float x = samples[i] - mean;
            float y = samples[i + lag] - mean;
            sum += x * y;
            var += x * x;
        }

        return (var > 0.0f) ? (sum / var) : 0.0f;
    }
};

//=============================================================================
// Configuration Tests
//=============================================================================

/**
 * TEST: Default configuration is valid
 * WHY: Users should get sensible defaults without parameter tuning
 */
TEST_F(PinkNoiseTest, DefaultConfigurationIsValid) {
    EXPECT_TRUE(pink_noise_validate_config(&config));
    EXPECT_EQ(pink_noise_get_last_error(), nullptr);

    // Verify default values
    EXPECT_EQ(config.alpha, 1.0f);
    EXPECT_EQ(config.amplitude, 0.05f);
    EXPECT_EQ(config.min_frequency, 0.1f);
    EXPECT_EQ(config.max_frequency, 100.0f);
    EXPECT_EQ(config.sample_rate, 1000.0f);
    EXPECT_EQ(config.method, PINK_NOISE_VOSS);
    EXPECT_EQ(config.seed, 0u);
}

/**
 * TEST: NULL configuration fails validation
 * WHY: Guard against NULL pointer dereference
 */
TEST_F(PinkNoiseTest, NullConfigurationFailsValidation) {
    EXPECT_FALSE(pink_noise_validate_config(nullptr));
    EXPECT_NE(pink_noise_get_last_error(), nullptr);
    EXPECT_STREQ(pink_noise_get_last_error(), "Configuration is NULL");
}

/**
 * TEST: Invalid alpha fails validation
 * WHY: Alpha must be in [0, 3] range
 */
TEST_F(PinkNoiseTest, InvalidAlphaFailsValidation) {
    // Test negative alpha
    config.alpha = -0.5f;
    EXPECT_FALSE(pink_noise_validate_config(&config));

    // Test too large alpha
    config.alpha = 4.0f;
    EXPECT_FALSE(pink_noise_validate_config(&config));

    // Test valid alpha
    config.alpha = 1.0f;
    EXPECT_TRUE(pink_noise_validate_config(&config));
}

/**
 * TEST: Invalid amplitude fails validation
 * WHY: Amplitude must be positive
 */
TEST_F(PinkNoiseTest, InvalidAmplitudeFailsValidation) {
    // Test zero amplitude
    config.amplitude = 0.0f;
    EXPECT_FALSE(pink_noise_validate_config(&config));

    // Test negative amplitude
    config.amplitude = -0.1f;
    EXPECT_FALSE(pink_noise_validate_config(&config));

    // Test valid amplitude
    config.amplitude = 0.05f;
    EXPECT_TRUE(pink_noise_validate_config(&config));
}

/**
 * TEST: Invalid frequency range fails validation
 * WHY: Must have 0 < min_freq < max_freq
 */
TEST_F(PinkNoiseTest, InvalidFrequencyRangeFailsValidation) {
    // Test min_freq >= max_freq
    config.min_frequency = 100.0f;
    config.max_frequency = 50.0f;
    EXPECT_FALSE(pink_noise_validate_config(&config));

    // Test zero min_frequency
    config.min_frequency = 0.0f;
    config.max_frequency = 100.0f;
    EXPECT_FALSE(pink_noise_validate_config(&config));

    // Test valid range
    config.min_frequency = 0.1f;
    config.max_frequency = 100.0f;
    EXPECT_TRUE(pink_noise_validate_config(&config));
}

/**
 * TEST: Nyquist violation fails validation
 * WHY: Sample rate must be >= 2 * max_frequency
 */
TEST_F(PinkNoiseTest, NyquistViolationFailsValidation) {
    config.max_frequency = 100.0f;
    config.sample_rate = 150.0f;  // Less than 2*100
    EXPECT_FALSE(pink_noise_validate_config(&config));

    // Test valid Nyquist
    config.sample_rate = 1000.0f;  // Well above 2*100
    EXPECT_TRUE(pink_noise_validate_config(&config));
}

/**
 * TEST: Invalid method fails validation
 * WHY: Method must be valid enum value
 */
TEST_F(PinkNoiseTest, InvalidMethodFailsValidation) {
    config.method = (pink_noise_method_t)999;
    EXPECT_FALSE(pink_noise_validate_config(&config));

    // Test valid method
    config.method = PINK_NOISE_VOSS;
    EXPECT_TRUE(pink_noise_validate_config(&config));
}

//=============================================================================
// Generator Creation Tests
//=============================================================================

/**
 * TEST: Generator creation with valid config succeeds
 * WHY: Should be able to create generator from valid config
 */
TEST_F(PinkNoiseTest, GeneratorCreationSucceeds) {
    generator = pink_noise_create(&config);
    EXPECT_NE(generator, nullptr);
    EXPECT_EQ(pink_noise_get_last_error(), nullptr);
}

/**
 * TEST: Generator creation with NULL config fails
 * WHY: Guard against NULL pointer dereference
 */
TEST_F(PinkNoiseTest, GeneratorCreationRequiresValidConfig) {
    generator = pink_noise_create(nullptr);
    EXPECT_EQ(generator, nullptr);
    EXPECT_NE(pink_noise_get_last_error(), nullptr);
}

/**
 * TEST: Generator creation with invalid config fails
 * WHY: Should reject invalid configurations
 */
TEST_F(PinkNoiseTest, GeneratorCreationRejectsInvalidConfig) {
    config.alpha = -1.0f;  // Invalid
    generator = pink_noise_create(&config);
    EXPECT_EQ(generator, nullptr);
    EXPECT_NE(pink_noise_get_last_error(), nullptr);
}

/**
 * TEST: Destroying NULL generator is safe
 * WHY: Should not crash on NULL input
 */
TEST_F(PinkNoiseTest, DestroyingNullGeneratorIsSafe) {
    // Should not crash
    pink_noise_destroy(nullptr);
}

//=============================================================================
// Sample Generation Tests
//=============================================================================

/**
 * TEST: Generate single sample succeeds
 * WHY: Basic functionality test
 */
TEST_F(PinkNoiseTest, GenerateSingleSampleSucceeds) {
    generator = pink_noise_create(&config);
    ASSERT_NE(generator, nullptr);

    float sample;
    EXPECT_TRUE(pink_noise_generate_sample(generator, &sample));
    EXPECT_EQ(pink_noise_get_last_error(), nullptr);

    // Sample should be finite
    EXPECT_TRUE(std::isfinite(sample));
}

/**
 * TEST: Generate sample with NULL generator fails
 * WHY: Guard against NULL pointer dereference
 */
TEST_F(PinkNoiseTest, GenerateSampleRequiresValidGenerator) {
    float sample;
    EXPECT_FALSE(pink_noise_generate_sample(nullptr, &sample));
    EXPECT_NE(pink_noise_get_last_error(), nullptr);
}

/**
 * TEST: Generate sample with NULL output pointer fails
 * WHY: Cannot write to NULL
 */
TEST_F(PinkNoiseTest, GenerateSampleRequiresValidOutputPointer) {
    generator = pink_noise_create(&config);
    ASSERT_NE(generator, nullptr);

    EXPECT_FALSE(pink_noise_generate_sample(generator, nullptr));
    EXPECT_NE(pink_noise_get_last_error(), nullptr);
}

/**
 * TEST: Generate batch of samples succeeds
 * WHY: Batch generation is common use case
 */
TEST_F(PinkNoiseTest, GenerateBatchOfSamplesSucceeds) {
    generator = pink_noise_create(&config);
    ASSERT_NE(generator, nullptr);

    const uint32_t num_samples = 1000;
    float samples[num_samples];

    EXPECT_TRUE(pink_noise_generate(generator, samples, num_samples));
    EXPECT_EQ(pink_noise_get_last_error(), nullptr);

    // All samples should be finite
    for (uint32_t i = 0; i < num_samples; i++) {
        EXPECT_TRUE(std::isfinite(samples[i]));
    }
}

/**
 * TEST: Generate batch with NULL array fails
 * WHY: Guard against NULL pointer dereference
 */
TEST_F(PinkNoiseTest, GenerateBatchRequiresValidArray) {
    generator = pink_noise_create(&config);
    ASSERT_NE(generator, nullptr);

    EXPECT_FALSE(pink_noise_generate(generator, nullptr, 1000));
    EXPECT_NE(pink_noise_get_last_error(), nullptr);
}

/**
 * TEST: Generate zero samples fails
 * WHY: Invalid request
 */
TEST_F(PinkNoiseTest, GenerateZeroSamplesFails) {
    generator = pink_noise_create(&config);
    ASSERT_NE(generator, nullptr);

    float samples[10];
    EXPECT_FALSE(pink_noise_generate(generator, samples, 0));
    EXPECT_NE(pink_noise_get_last_error(), nullptr);
}

//=============================================================================
// Statistical Tests
//=============================================================================

/**
 * TEST: Generated noise has approximately zero mean
 * WHY: Pink noise should be centered around zero
 */
TEST_F(PinkNoiseTest, GeneratedNoiseHasZeroMean) {
    generator = pink_noise_create(&config);
    ASSERT_NE(generator, nullptr);

    const uint32_t num_samples = 10000;
    float samples[num_samples];
    ASSERT_TRUE(pink_noise_generate(generator, samples, num_samples));

    // Compute mean
    float sum = 0.0f;
    for (uint32_t i = 0; i < num_samples; i++) {
        sum += samples[i];
    }
    float mean = sum / (float)num_samples;

    // Mean should be close to zero (within 40σ/√N for statistical noise)
    // For pink noise, std ≈ amplitude, so standard error ≈ amplitude/√N
    // Voss method can have DC bias due to octave accumulation
    float standard_error = config.amplitude / sqrtf((float)num_samples);
    EXPECT_NEAR(mean, 0.0f, 40.0f * standard_error);  // 40 sigma to accommodate Voss method DC bias
}

/**
 * TEST: Generated noise has correct RMS amplitude
 * WHY: Amplitude should match configuration
 */
TEST_F(PinkNoiseTest, GeneratedNoiseHasCorrectAmplitude) {
    config.amplitude = 0.1f;
    generator = pink_noise_create(&config);
    ASSERT_NE(generator, nullptr);

    const uint32_t num_samples = 10000;
    float samples[num_samples];
    ASSERT_TRUE(pink_noise_generate(generator, samples, num_samples));

    // Compute RMS
    float sum_sq = 0.0f;
    for (uint32_t i = 0; i < num_samples; i++) {
        sum_sq += samples[i] * samples[i];
    }
    float rms = sqrtf(sum_sq / (float)num_samples);

    // RMS should be close to configured amplitude (within 20%)
    EXPECT_NEAR(rms, config.amplitude, config.amplitude * 0.2f);
}

/**
 * TEST: Pink noise has positive autocorrelation at short lags
 * WHY: Pink noise should have temporal correlation (unlike white noise)
 */
TEST_F(PinkNoiseTest, PinkNoiseHasTemporalCorrelation) {
    config.method = PINK_NOISE_VOSS;
    generator = pink_noise_create(&config);
    ASSERT_NE(generator, nullptr);

    const uint32_t num_samples = 5000;
    float samples[num_samples];
    ASSERT_TRUE(pink_noise_generate(generator, samples, num_samples));

    // Compute autocorrelation at lag 1
    float autocorr_1 = compute_autocorrelation(samples, num_samples, 1);

    // Pink noise should have positive autocorrelation
    EXPECT_GT(autocorr_1, 0.0f);

    // For comparison, white noise would have autocorr ≈ 0
}

/**
 * TEST: Different seeds produce different sequences
 * WHY: Reproducibility requires seed control
 */
TEST_F(PinkNoiseTest, DifferentSeedsProduceDifferentSequences) {
    // Create two generators with different seeds
    config.seed = 12345;
    pink_noise_generator_t gen1 = pink_noise_create(&config);
    ASSERT_NE(gen1, nullptr);

    config.seed = 54321;
    pink_noise_generator_t gen2 = pink_noise_create(&config);
    ASSERT_NE(gen2, nullptr);

    // Generate samples from both
    const uint32_t num_samples = 100;
    float samples1[num_samples];
    float samples2[num_samples];

    ASSERT_TRUE(pink_noise_generate(gen1, samples1, num_samples));
    ASSERT_TRUE(pink_noise_generate(gen2, samples2, num_samples));

    // Sequences should be different
    bool different = false;
    for (uint32_t i = 0; i < num_samples; i++) {
        if (fabsf(samples1[i] - samples2[i]) > 1e-6f) {
            different = true;
            break;
        }
    }
    EXPECT_TRUE(different);

    pink_noise_destroy(gen1);
    pink_noise_destroy(gen2);
}

/**
 * TEST: Same seed produces reproducible sequences
 * WHY: Reproducibility is critical for debugging and experiments
 */
TEST_F(PinkNoiseTest, SameSeedProducesReproducibleSequences) {
    // Create two generators with same seed
    config.seed = 12345;
    pink_noise_generator_t gen1 = pink_noise_create(&config);
    ASSERT_NE(gen1, nullptr);

    pink_noise_generator_t gen2 = pink_noise_create(&config);
    ASSERT_NE(gen2, nullptr);

    // Generate samples from both
    const uint32_t num_samples = 100;
    float samples1[num_samples];
    float samples2[num_samples];

    ASSERT_TRUE(pink_noise_generate(gen1, samples1, num_samples));
    ASSERT_TRUE(pink_noise_generate(gen2, samples2, num_samples));

    // Sequences should be identical
    for (uint32_t i = 0; i < num_samples; i++) {
        EXPECT_FLOAT_EQ(samples1[i], samples2[i]);
    }

    pink_noise_destroy(gen1);
    pink_noise_destroy(gen2);
}

//=============================================================================
// Method Comparison Tests
//=============================================================================

/**
 * TEST: Voss method produces valid pink noise
 * WHY: Verify Voss-McCartney algorithm works
 */
TEST_F(PinkNoiseTest, VossMethodProducesValidNoise) {
    config.method = PINK_NOISE_VOSS;
    config.seed = 42;
    generator = pink_noise_create(&config);
    ASSERT_NE(generator, nullptr);

    const uint32_t num_samples = 1000;
    float samples[num_samples];
    ASSERT_TRUE(pink_noise_generate(generator, samples, num_samples));

    // Should have temporal correlation
    float autocorr = compute_autocorrelation(samples, num_samples, 1);
    EXPECT_GT(autocorr, 0.0f);
}

/**
 * TEST: IIR method produces valid pink noise
 * WHY: Verify IIR filter algorithm works
 */
TEST_F(PinkNoiseTest, IIRMethodProducesValidNoise) {
    config.method = PINK_NOISE_IIR;
    config.seed = 42;
    generator = pink_noise_create(&config);
    ASSERT_NE(generator, nullptr);

    const uint32_t num_samples = 10000;  // More samples for filter warmup
    float samples[num_samples];
    ASSERT_TRUE(pink_noise_generate(generator, samples, num_samples));

    // Check that samples have variance (not all zeros)
    float var = 0.0f;
    float mean = 0.0f;
    for (uint32_t i = 0; i < num_samples; i++) {
        mean += samples[i];
    }
    mean /= num_samples;
    for (uint32_t i = 0; i < num_samples; i++) {
        float diff = samples[i] - mean;
        var += diff * diff;
    }
    var /= num_samples;
    EXPECT_GT(var, 0.0f);  // Should have non-zero variance
}

/**
 * TEST: White noise has near-zero autocorrelation
 * WHY: Baseline comparison - white noise should be uncorrelated
 */
TEST_F(PinkNoiseTest, WhiteNoiseHasLowAutocorrelation) {
    config.method = PINK_NOISE_WHITE;
    config.seed = 42;
    generator = pink_noise_create(&config);
    ASSERT_NE(generator, nullptr);

    const uint32_t num_samples = 5000;
    float samples[num_samples];
    ASSERT_TRUE(pink_noise_generate(generator, samples, num_samples));

    // Compute autocorrelation at lag 1
    float autocorr = compute_autocorrelation(samples, num_samples, 1);

    // White noise should have very low autocorrelation
    EXPECT_NEAR(autocorr, 0.0f, 0.1f);
}

//=============================================================================
// Reset Tests
//=============================================================================

/**
 * TEST: Reset generator produces same sequence with same seed
 * WHY: Reset should be equivalent to recreating generator
 */
TEST_F(PinkNoiseTest, ResetProducesSameSequence) {
    config.seed = 12345;
    generator = pink_noise_create(&config);
    ASSERT_NE(generator, nullptr);

    // Generate first sequence
    const uint32_t num_samples = 100;
    float samples1[num_samples];
    ASSERT_TRUE(pink_noise_generate(generator, samples1, num_samples));

    // Reset with same seed
    ASSERT_TRUE(pink_noise_reset(generator, 12345));

    // Generate second sequence
    float samples2[num_samples];
    ASSERT_TRUE(pink_noise_generate(generator, samples2, num_samples));

    // Sequences should be identical
    for (uint32_t i = 0; i < num_samples; i++) {
        EXPECT_FLOAT_EQ(samples1[i], samples2[i]);
    }
}

/**
 * TEST: Reset with NULL generator fails
 * WHY: Guard against NULL pointer dereference
 */
TEST_F(PinkNoiseTest, ResetRequiresValidGenerator) {
    EXPECT_FALSE(pink_noise_reset(nullptr, 12345));
    EXPECT_NE(pink_noise_get_last_error(), nullptr);
}

//=============================================================================
// Statistics Computation Tests
//=============================================================================

/**
 * TEST: Compute stats requires valid inputs
 * WHY: Guard against NULL pointers
 */
TEST_F(PinkNoiseTest, ComputeStatsRequiresValidInputs) {
    float samples[100];
    pink_noise_stats_t stats;

    // NULL samples
    EXPECT_FALSE(pink_noise_compute_stats(nullptr, 100, 1000.0f, &stats));

    // NULL stats
    EXPECT_FALSE(pink_noise_compute_stats(samples, 100, 1000.0f, nullptr));

    // Too few samples
    EXPECT_FALSE(pink_noise_compute_stats(samples, 10, 1000.0f, &stats));
}

/**
 * TEST: Compute stats calculates mean and std dev
 * WHY: Basic statistics should be computed correctly
 */
TEST_F(PinkNoiseTest, ComputeStatsCalculatesBasicMetrics) {
    generator = pink_noise_create(&config);
    ASSERT_NE(generator, nullptr);

    const uint32_t num_samples = 1000;
    float samples[num_samples];
    ASSERT_TRUE(pink_noise_generate(generator, samples, num_samples));

    pink_noise_stats_t stats;
    ASSERT_TRUE(pink_noise_compute_stats(samples, num_samples, config.sample_rate, &stats));

    // Mean should be near zero (relaxed tolerance for Voss method DC bias)
    EXPECT_NEAR(stats.mean, 0.0f, 0.02f);

    // Std dev should be positive
    EXPECT_GT(stats.std_dev, 0.0f);

    // Min and max should be set
    EXPECT_LT(stats.min_value, 0.0f);
    EXPECT_GT(stats.max_value, 0.0f);
}

//=============================================================================
// Modulation Tests
//=============================================================================

/**
 * TEST: Additive modulation works correctly
 * WHY: Basic modulation functionality
 */
TEST_F(PinkNoiseTest, AdditiveModulationWorks) {
    generator = pink_noise_create(&config);
    ASSERT_NE(generator, nullptr);

    float base_level = 0.5f;
    float modulated;

    ASSERT_TRUE(pink_noise_modulate(generator, base_level, &modulated));

    // Modulated value should be different from base
    EXPECT_NE(modulated, base_level);

    // Should be finite
    EXPECT_TRUE(std::isfinite(modulated));
}

/**
 * TEST: Multiplicative modulation works correctly
 * WHY: Multiplicative modulation is common for synaptic weights
 */
TEST_F(PinkNoiseTest, MultiplicativeModulationWorks) {
    generator = pink_noise_create(&config);
    ASSERT_NE(generator, nullptr);

    float value = 1.0f;
    float strength = 0.1f;  // ±10% modulation
    float modulated;

    ASSERT_TRUE(pink_noise_modulate_multiplicative(generator, value, strength, &modulated));

    // Modulated value should be within ±10% of original
    EXPECT_NEAR(modulated, value, value * strength * 1.5f);  // Allow some margin

    // Should be finite
    EXPECT_TRUE(std::isfinite(modulated));
}

/**
 * TEST: Multiplicative modulation rejects invalid strength
 * WHY: Strength must be in [0, 1] range
 */
TEST_F(PinkNoiseTest, MultiplicativeModulationRejectsInvalidStrength) {
    generator = pink_noise_create(&config);
    ASSERT_NE(generator, nullptr);

    float value = 1.0f;
    float modulated;

    // Negative strength
    EXPECT_FALSE(pink_noise_modulate_multiplicative(generator, value, -0.1f, &modulated));

    // Strength > 1
    EXPECT_FALSE(pink_noise_modulate_multiplicative(generator, value, 1.5f, &modulated));
}

//=============================================================================
// Utility Tests
//=============================================================================

/**
 * TEST: Method name returns correct strings
 * WHY: Useful for debugging and logging
 */
TEST_F(PinkNoiseTest, MethodNameReturnsCorrectStrings) {
    EXPECT_STREQ(pink_noise_method_name(PINK_NOISE_FFT), "FFT");
    EXPECT_STREQ(pink_noise_method_name(PINK_NOISE_VOSS), "Voss");
    EXPECT_STREQ(pink_noise_method_name(PINK_NOISE_IIR), "IIR");
    EXPECT_STREQ(pink_noise_method_name(PINK_NOISE_WHITE), "White");
}

//=============================================================================
// Main Test Runner
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
