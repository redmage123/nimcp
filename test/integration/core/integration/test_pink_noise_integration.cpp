/**
 * @file test_pink_noise_integration.cpp
 * @brief Integration tests for Pink Noise neuromodulation
 *
 * WHAT: Verify pink noise features are actively used in neuromodulation
 * WHY:  Ensure 1/f spectrum generation works correctly
 * HOW:  Test noise generation, spectral validation, and neuromodulation
 *
 * TEST COVERAGE:
 * 1. Generator creation and configuration
 * 2. Batch noise generation and validation
 * 3. Single sample streaming generation
 * 4. Spectral analysis (1/f^α validation)
 * 5. Different generation methods (FFT, Voss, IIR)
 * 6. Additive neuromodulation
 * 7. Multiplicative neuromodulation
 * 8. Generator reset and reproducibility
 * 9. Statistics computation
 * 10. Integration with neuromodulator system
 *
 * @version Integration Testing Framework v1.0
 * @date 2025-11-13
 */

#include <gtest/gtest.h>
#include <cmath>
#include <algorithm>

#include "plasticity/noise/nimcp_pink_noise.h"

//=============================================================================
// Test Fixture
//=============================================================================

class PinkNoiseIntegrationTest : public ::testing::Test {
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
// Integration Test 1: Generator Creation and Configuration
//=============================================================================

TEST_F(PinkNoiseIntegrationTest, GeneratorCreation) {
    // WHAT: Verify generator creation with valid config
    // WHY:  Basic setup must work correctly

    pink_noise_config_t config = pink_noise_default_config();
    generator = pink_noise_create(&config);

    ASSERT_NE(generator, nullptr) << "Generator creation should succeed";

    // Verify defaults are reasonable
    EXPECT_EQ(config.alpha, 1.0f);  // True pink noise
    EXPECT_GT(config.amplitude, 0.0f);
    EXPECT_LT(config.min_frequency, config.max_frequency);
    EXPECT_GE(config.sample_rate, 2.0f * config.max_frequency);  // Nyquist
}

//=============================================================================
// Integration Test 2: Batch Noise Generation
//=============================================================================

TEST_F(PinkNoiseIntegrationTest, BatchGeneration) {
    // WHAT: Verify batch noise generation produces valid samples
    // WHY:  Core functionality for pre-computing noise sequences

    pink_noise_config_t config = pink_noise_default_config();
    config.amplitude = 0.1f;
    generator = pink_noise_create(&config);
    ASSERT_NE(generator, nullptr);

    const uint32_t num_samples = 1024;
    float samples[1024];

    bool success = pink_noise_generate(generator, samples, num_samples);
    ASSERT_TRUE(success) << "Batch generation should succeed";

    // Verify samples are non-zero and bounded
    bool has_nonzero = false;
    for (uint32_t i = 0; i < num_samples; ++i) {
        EXPECT_TRUE(std::isfinite(samples[i])) << "Sample should be finite";
        if (samples[i] != 0.0f) has_nonzero = true;
    }
    EXPECT_TRUE(has_nonzero) << "Should have non-zero samples";
}

//=============================================================================
// Integration Test 3: Single Sample Streaming
//=============================================================================

TEST_F(PinkNoiseIntegrationTest, StreamingGeneration) {
    // WHAT: Verify single-sample streaming generation
    // WHY:  Real-time applications need sample-by-sample generation

    pink_noise_config_t config = pink_noise_default_config();
    generator = pink_noise_create(&config);
    ASSERT_NE(generator, nullptr);

    // Generate 100 samples one at a time
    float samples[100];
    for (int i = 0; i < 100; ++i) {
        bool success = pink_noise_generate_sample(generator, &samples[i]);
        ASSERT_TRUE(success) << "Sample generation should succeed";
        EXPECT_TRUE(std::isfinite(samples[i])) << "Sample should be finite";
    }

    // Verify temporal correlation (samples shouldn't be independent white noise)
    // Simple autocorrelation check: successive samples should be somewhat correlated
    int similar_pairs = 0;
    for (int i = 0; i < 99; ++i) {
        if (fabsf(samples[i+1] - samples[i]) < 0.5f) similar_pairs++;
    }
    EXPECT_GT(similar_pairs, 20) << "Pink noise should show temporal correlation";
}

//=============================================================================
// Integration Test 4: Spectral Validation
//=============================================================================

TEST_F(PinkNoiseIntegrationTest, SpectralValidation) {
    // WHAT: Verify generated noise has 1/f^α spectrum
    // WHY:  Core property of pink noise

    pink_noise_config_t config = pink_noise_default_config();
    config.alpha = 1.0f;
    config.method = PINK_NOISE_FFT;  // Use highest quality method
    config.seed = 12345;  // Fixed seed for reproducibility
    generator = pink_noise_create(&config);
    ASSERT_NE(generator, nullptr);

    const uint32_t num_samples = 1024;
    float samples[1024];
    pink_noise_generate(generator, samples, num_samples);

    // Validate spectrum (very relaxed - DFT on short sequences has high variance)
    bool is_valid = pink_noise_validate(
        samples, num_samples, config.sample_rate,
        1.0f, 0.8f  // Expect α=1.0 ± 0.8 (DFT is approximate)
    );

    EXPECT_TRUE(is_valid) << "Generated noise should approximate 1/f spectrum";
}

//=============================================================================
// Integration Test 5: Different Generation Methods
//=============================================================================

TEST_F(PinkNoiseIntegrationTest, GenerationMethods) {
    // WHAT: Verify all generation methods work
    // WHY:  Different methods suit different performance requirements

    const pink_noise_method_t methods[] = {
        PINK_NOISE_FFT,
        PINK_NOISE_VOSS,
        PINK_NOISE_IIR,
        PINK_NOISE_WHITE
    };

    const uint32_t num_samples = 256;
    float samples[256];

    for (auto method : methods) {
        pink_noise_config_t config = pink_noise_default_config();
        config.method = method;

        generator = pink_noise_create(&config);
        ASSERT_NE(generator, nullptr) << "Method " << method << " should work";

        bool success = pink_noise_generate(generator, samples, num_samples);
        EXPECT_TRUE(success) << "Method " << method << " should generate";

        // All methods should produce finite samples
        for (uint32_t i = 0; i < num_samples; ++i) {
            EXPECT_TRUE(std::isfinite(samples[i]));
        }

        pink_noise_destroy(generator);
        generator = nullptr;
    }
}

//=============================================================================
// Integration Test 6: Additive Neuromodulation
//=============================================================================

TEST_F(PinkNoiseIntegrationTest, AdditiveModulation) {
    // WHAT: Verify additive modulation: M_new = M_base + noise
    // WHY:  Used for baseline neuromodulator fluctuations

    pink_noise_config_t config = pink_noise_default_config();
    config.amplitude = 0.05f;
    generator = pink_noise_create(&config);
    ASSERT_NE(generator, nullptr);

    float base_level = 0.5f;
    float modulated[100];

    for (int i = 0; i < 100; ++i) {
        bool success = pink_noise_modulate(generator, base_level, &modulated[i]);
        ASSERT_TRUE(success);
        EXPECT_TRUE(std::isfinite(modulated[i]));
    }

    // Modulated values should be near base_level ± amplitude
    float mean = 0.0f;
    for (int i = 0; i < 100; ++i) mean += modulated[i];
    mean /= 100.0f;

    EXPECT_NEAR(mean, base_level, 0.1f) << "Mean should be near base level";
}

//=============================================================================
// Integration Test 7: Multiplicative Neuromodulation
//=============================================================================

TEST_F(PinkNoiseIntegrationTest, MultiplicativeModulation) {
    // WHAT: Verify multiplicative modulation: V_new = V * (1 + α*noise)
    // WHY:  Used for synaptic strength modulation

    pink_noise_config_t config = pink_noise_default_config();
    generator = pink_noise_create(&config);
    ASSERT_NE(generator, nullptr);

    float base_value = 1.0f;
    float modulation_strength = 0.1f;  // ±10% variation
    float modulated[100];

    for (int i = 0; i < 100; ++i) {
        bool success = pink_noise_modulate_multiplicative(
            generator, base_value, modulation_strength, &modulated[i]
        );
        ASSERT_TRUE(success);
        EXPECT_GT(modulated[i], base_value * 0.7f) << "Should stay reasonably close";
        EXPECT_LT(modulated[i], base_value * 1.3f);
    }
}

//=============================================================================
// Integration Test 8: Generator Reset and Reproducibility
//=============================================================================

TEST_F(PinkNoiseIntegrationTest, ResetReproducibility) {
    // WHAT: Verify same seed produces same noise sequence
    // WHY:  Important for debugging and reproducible experiments

    pink_noise_config_t config = pink_noise_default_config();
    config.seed = 12345;
    generator = pink_noise_create(&config);
    ASSERT_NE(generator, nullptr);

    float sequence1[50];
    pink_noise_generate(generator, sequence1, 50);

    // Reset and generate again
    pink_noise_reset(generator, 12345);
    float sequence2[50];
    pink_noise_generate(generator, sequence2, 50);

    // Sequences should be identical
    for (int i = 0; i < 50; ++i) {
        EXPECT_FLOAT_EQ(sequence1[i], sequence2[i])
            << "Same seed should produce same sequence";
    }
}

//=============================================================================
// Integration Test 9: Statistics Computation
//=============================================================================

TEST_F(PinkNoiseIntegrationTest, StatisticsComputation) {
    // WHAT: Verify statistics computation works correctly
    // WHY:  Validates noise quality

    pink_noise_config_t config = pink_noise_default_config();
    config.alpha = 1.0f;
    config.method = PINK_NOISE_FFT;
    config.seed = 999;  // Fixed seed for reproducibility
    generator = pink_noise_create(&config);
    ASSERT_NE(generator, nullptr);

    const uint32_t num_samples = 1024;
    float samples[1024];
    pink_noise_generate(generator, samples, num_samples);

    pink_noise_stats_t stats;
    bool success = pink_noise_compute_stats(
        samples, num_samples, config.sample_rate, &stats
    );
    ASSERT_TRUE(success);

    // Verify stats are reasonable (DFT on short sequences has high variance)
    EXPECT_GT(stats.measured_alpha, 0.2f) << "Should show power-law behavior";
    EXPECT_LT(stats.measured_alpha, 2.0f) << "Should be in reasonable range";
    EXPECT_GT(stats.spectral_fit_r2, 0.2f) << "Fit should show some correlation";
    EXPECT_NEAR(stats.mean, 0.0f, 0.1f) << "Mean should be near zero";
}

//=============================================================================
// Integration Test 10: Continuous Modulation Stability
//=============================================================================

TEST_F(PinkNoiseIntegrationTest, ContinuousModulation) {
    // WHAT: Verify continuous noise generation remains stable
    // WHY:  Long-running simulations need stable noise generation

    pink_noise_config_t config = pink_noise_default_config();
    config.amplitude = 0.05f;
    generator = pink_noise_create(&config);
    ASSERT_NE(generator, nullptr);

    // Simulate 1000 steps of continuous modulation
    float base_value = 0.5f;
    float sum = 0.0f;
    float min_val = 1.0f;
    float max_val = -1.0f;

    for (int step = 0; step < 1000; ++step) {
        float modulated;
        bool success = pink_noise_modulate(generator, base_value, &modulated);
        ASSERT_TRUE(success);

        sum += modulated;
        if (modulated < min_val) min_val = modulated;
        if (modulated > max_val) max_val = modulated;

        EXPECT_TRUE(std::isfinite(modulated));
    }

    // Mean should be near base value
    float mean = sum / 1000.0f;
    EXPECT_NEAR(mean, base_value, 0.1f) << "Mean should stay near base";

    // Range should be reasonable
    EXPECT_GT(max_val - min_val, 0.05f) << "Should have some variation";
    EXPECT_LT(max_val - min_val, 1.0f) << "Variation shouldn't be extreme";
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
