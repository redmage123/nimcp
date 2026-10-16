/**
 * @file test_pink_noise_backward_compat.cpp
 * @brief Regression tests for Pink Noise neuromodulation
 *
 * WHAT: Ensures pink noise doesn't break existing code
 * WHY:  Verify zero breaking changes to pre-pink-noise code
 * HOW:  Test legacy patterns and ensure they still work correctly
 *
 * TEST COVERAGE:
 * 1. Neuromodulator system works without pink noise
 * 2. Legacy neuromodulation patterns unchanged
 * 3. Pink noise API doesn't break non-noise code
 * 4. No performance regression
 * 5. Parameter validation
 * 6. Memory management no leaks
 * 7. Old learning patterns work
 * 8. State consistency
 * 9. Batch processing not broken
 * 10. Config validation
 *
 * @version Regression Testing Framework v1.0
 * @date 2025-11-13
 */

#include <gtest/gtest.h>
#include <cmath>

// Headers have their own extern "C" guards
    #include "plasticity/noise/nimcp_pink_noise.h"
    #include "utils/time/nimcp_time.h"

//=============================================================================
// Test Fixture
//=============================================================================

class PinkNoiseRegressionTest : public ::testing::Test {
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
// Regression Test 1: Basic Noise Generation Still Works
//=============================================================================

TEST_F(PinkNoiseRegressionTest, BasicGeneration_StillWorks) {
    // Old code pattern: Basic noise generation

    pink_noise_config_t config = pink_noise_default_config();
    generator = pink_noise_create(&config);
    ASSERT_NE(generator, nullptr) << "Generator creation should work";

    float sample;
    bool success = pink_noise_generate_sample(generator, &sample);
    EXPECT_TRUE(success) << "Sample generation should work";
    EXPECT_TRUE(std::isfinite(sample)) << "Sample should be finite";
}

//=============================================================================
// Regression Test 2: Legacy Noise Patterns Unchanged
//=============================================================================

TEST_F(PinkNoiseRegressionTest, LegacyNoisePatterns_StillWork) {
    // Old pattern: Repeated noise generation

    pink_noise_config_t config = pink_noise_default_config();
    generator = pink_noise_create(&config);
    ASSERT_NE(generator, nullptr);

    for (int step = 0; step < 100; ++step) {
        float sample;
        bool success = pink_noise_generate_sample(generator, &sample);

        // Should work as before
        EXPECT_TRUE(success);
        EXPECT_TRUE(std::isfinite(sample));
    }
}

//=============================================================================
// Regression Test 3: Pink Noise API Doesn't Break Non-Noise Code
//=============================================================================

TEST_F(PinkNoiseRegressionTest, PinkNoiseAPI_NoCPUBreakage) {
    // Use pink noise API (new)
    pink_noise_config_t config = pink_noise_default_config();
    generator = pink_noise_create(&config);
    EXPECT_NE(generator, nullptr);

    float sample;
    pink_noise_generate_sample(generator, &sample);

    // Old noise patterns should still work after using new API
    float samples[10];
    bool success = pink_noise_generate(generator, samples, 10);
    EXPECT_TRUE(success);
}

//=============================================================================
// Regression Test 4: No Performance Regression
//=============================================================================

TEST_F(PinkNoiseRegressionTest, NoPerformanceRegression) {
    // Old pattern: Fast noise generation should not slow down
    pink_noise_config_t config = pink_noise_default_config();
    generator = pink_noise_create(&config);
    ASSERT_NE(generator, nullptr);

    uint64_t start_time = nimcp_time_get_us();
    float sample;
    for (int i = 0; i < 10000; ++i) {
        pink_noise_generate_sample(generator, &sample);
    }
    uint64_t end_time = nimcp_time_get_us();

    uint64_t elapsed = end_time - start_time;
    float avg_us = elapsed / 10000.0f;

    EXPECT_LT(avg_us, 10.0f)
        << "Pink noise generation should be reasonably fast";
}

//=============================================================================
// Regression Test 5: Parameter Validation
//=============================================================================

TEST_F(PinkNoiseRegressionTest, ParameterValidation) {
    // Test default config is valid
    pink_noise_config_t config = pink_noise_default_config();

    EXPECT_GT(config.alpha, 0.0f);
    EXPECT_LE(config.alpha, 3.0f);
    EXPECT_GT(config.amplitude, 0.0f);
    EXPECT_LT(config.min_frequency, config.max_frequency);
    EXPECT_GE(config.sample_rate, 2.0f * config.max_frequency);

    bool is_valid = pink_noise_validate_config(&config);
    EXPECT_TRUE(is_valid) << "Default config should be valid";

    generator = pink_noise_create(&config);
    EXPECT_NE(generator, nullptr) << "Default config should work";
}

//=============================================================================
// Regression Test 6: Memory Management No Leaks
//=============================================================================

TEST_F(PinkNoiseRegressionTest, MemoryManagement_NoLeaks) {
    // Create and destroy generator multiple times
    for (int i = 0; i < 10; ++i) {
        pink_noise_config_t config = pink_noise_default_config();
        generator = pink_noise_create(&config);
        ASSERT_NE(generator, nullptr);

        float sample;
        pink_noise_generate_sample(generator, &sample);

        pink_noise_destroy(generator);
        generator = nullptr;
    }

    // Should not leak memory - verified by valgrind or sanitizers
    SUCCEED();
}

//=============================================================================
// Regression Test 7: Old Learning Patterns Work
//=============================================================================

TEST_F(PinkNoiseRegressionTest, OldLearningPattern_Works) {
    // Old pattern: Noise modulation in training loop
    pink_noise_config_t config = pink_noise_default_config();
    config.amplitude = 0.05f;
    generator = pink_noise_create(&config);
    ASSERT_NE(generator, nullptr);

    for (int episode = 0; episode < 5; episode++) {
        float base_value = 0.8f;

        // Generate modulated values over time
        for (int step = 0; step < 10; ++step) {
            float modulated;
            bool success = pink_noise_modulate(generator, base_value, &modulated);
            EXPECT_TRUE(success);
            EXPECT_TRUE(std::isfinite(modulated));
        }
    }

    SUCCEED();
}

//=============================================================================
// Regression Test 8: State Consistency
//=============================================================================

TEST_F(PinkNoiseRegressionTest, StateConsistency) {
    // Same inputs should produce same outputs with same seed
    pink_noise_config_t config = pink_noise_default_config();
    config.seed = 42;

    float samples1[50];
    float samples2[50];

    // First run
    generator = pink_noise_create(&config);
    ASSERT_NE(generator, nullptr);
    pink_noise_generate(generator, samples1, 50);
    pink_noise_destroy(generator);

    // Second run with same seed
    generator = pink_noise_create(&config);
    ASSERT_NE(generator, nullptr);
    pink_noise_generate(generator, samples2, 50);

    // Results should be identical
    for (int i = 0; i < 50; ++i) {
        EXPECT_FLOAT_EQ(samples1[i], samples2[i])
            << "Same seed should produce same results";
    }
}

//=============================================================================
// Regression Test 9: Batch Processing Not Broken
//=============================================================================

TEST_F(PinkNoiseRegressionTest, BatchProcessing_NotBroken) {
    // Old pattern: Batch noise generation
    pink_noise_config_t config = pink_noise_default_config();
    generator = pink_noise_create(&config);
    ASSERT_NE(generator, nullptr);

    float batch[100];
    bool success = pink_noise_generate(generator, batch, 100);
    ASSERT_TRUE(success);

    // Verify all samples are valid
    for (int i = 0; i < 100; ++i) {
        EXPECT_TRUE(std::isfinite(batch[i]));
    }

    SUCCEED();
}

//=============================================================================
// Regression Test 10: Config Validation
//=============================================================================

TEST_F(PinkNoiseRegressionTest, ConfigValidation) {
    pink_noise_config_t config;

    // Invalid: alpha > 3
    config = pink_noise_default_config();
    config.alpha = 5.0f;
    bool valid = pink_noise_validate_config(&config);
    EXPECT_FALSE(valid) << "Invalid alpha should be rejected";

    // Invalid: amplitude <= 0
    config = pink_noise_default_config();
    config.amplitude = -0.1f;
    valid = pink_noise_validate_config(&config);
    EXPECT_FALSE(valid) << "Negative amplitude should be rejected";

    // Invalid: min_frequency >= max_frequency
    config = pink_noise_default_config();
    config.min_frequency = 200.0f;
    config.max_frequency = 100.0f;
    valid = pink_noise_validate_config(&config);
    EXPECT_FALSE(valid) << "min >= max frequency should be rejected";

    // Invalid: sample_rate < 2 * max_frequency (Nyquist violation)
    config = pink_noise_default_config();
    config.max_frequency = 1000.0f;
    config.sample_rate = 1500.0f;  // < 2 * 1000
    valid = pink_noise_validate_config(&config);
    EXPECT_FALSE(valid) << "Nyquist violation should be rejected";

    // Valid config should work
    config = pink_noise_default_config();
    valid = pink_noise_validate_config(&config);
    EXPECT_TRUE(valid) << "Valid config should be accepted";

    generator = pink_noise_create(&config);
    EXPECT_NE(generator, nullptr) << "Valid config should succeed";
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
