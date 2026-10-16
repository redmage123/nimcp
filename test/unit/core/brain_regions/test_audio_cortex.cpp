/**
 * @file test_audio_cortex.cpp
 * @brief Unit tests for Audio Cortex bidirectional functions (Phase 10.11.3)
 *
 * WHAT: Tests for audio-speech bidirectional feedback
 * WHY:  Validate audio cortex bidirectional API
 * HOW:  Test speech salience detection and speech mode activation
 *
 * @author NIMCP Phase 10.11.3
 * @date 2025-11-10
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>

    #include "perception/nimcp_audio_cortex.h"

// =============================================================================
// TEST FIXTURES
// =============================================================================

class AudioCortexBidirectionalTest : public ::testing::Test {
protected:
    audio_cortex_t* cortex;

    void SetUp() override {
        audio_cortex_config_t config = {};
        config.sample_rate = 16000;
        config.frame_size = 512;
        config.num_freq_bins = 256;
        config.num_mel_filters = 40;
        config.num_mfcc = 13;
        config.num_channels = 1;
        config.feature_dim = 13;
        config.enable_attention = false;
        config.enable_memory = false;
        config.enable_fractal_topology = false;

        cortex = audio_cortex_create(&config);
        ASSERT_NE(cortex, nullptr) << "Failed to create audio cortex";
    }

    void TearDown() override {
        if (cortex) {
            audio_cortex_destroy(cortex);
            cortex = nullptr;
        }
    }

    // Helper: Generate speech-like features (mid-range activations with formant peaks)
    std::vector<float> generate_speech_features(uint32_t size) {
        std::vector<float> features(size);
        // Create formant-like peaks at specific bins (2-4 peaks typical for speech)
        for (uint32_t i = 0; i < size; i++) {
            features[i] = 0.2f;  // Baseline
            // Add peaks at positions 2, 5, 9 to simulate formants
            if (i == 2 || i == 5 || i == 9) {
                features[i] = 2.5f;  // Strong peak
            } else if (i == 3 || i == 6 || i == 10) {
                features[i] = 1.2f;  // Adjacent bins
            }
        }
        return features;
    }

    // Helper: Generate noise features (random uniform)
    std::vector<float> generate_noise_features(uint32_t size) {
        std::vector<float> features(size);
        for (uint32_t i = 0; i < size; i++) {
            // Noise is random, less structured
            features[i] = static_cast<float>(rand()) / RAND_MAX;
        }
        return features;
    }

    // Helper: Generate silence features (all zeros)
    std::vector<float> generate_silence_features(uint32_t size) {
        return std::vector<float>(size, 0.0f);
    }
};

// =============================================================================
// SPEECH SALIENCE TESTS
// =============================================================================

TEST_F(AudioCortexBidirectionalTest, GetSpeechSalienceNullCortex) {
    std::vector<float> features = generate_speech_features(13);
    float salience = audio_cortex_get_speech_salience(nullptr, features.data(), features.size());
    EXPECT_EQ(salience, 0.0f) << "Null cortex should return 0 salience";
}

TEST_F(AudioCortexBidirectionalTest, GetSpeechSalienceNullFeatures) {
    float salience = audio_cortex_get_speech_salience(cortex, nullptr, 13);
    EXPECT_EQ(salience, 0.0f) << "Null features should return 0 salience";
}

TEST_F(AudioCortexBidirectionalTest, GetSpeechSalienceZeroFeatures) {
    std::vector<float> features = generate_speech_features(13);
    float salience = audio_cortex_get_speech_salience(cortex, features.data(), 0);
    EXPECT_EQ(salience, 0.0f) << "Zero features should return 0 salience";
}

TEST_F(AudioCortexBidirectionalTest, GetSpeechSalienceSilence) {
    std::vector<float> features = generate_silence_features(13);
    float salience = audio_cortex_get_speech_salience(cortex, features.data(), features.size());
    EXPECT_EQ(salience, 0.0f) << "Silence should return 0 salience";
}

TEST_F(AudioCortexBidirectionalTest, GetSpeechSalienceSpeechLike) {
    std::vector<float> features = generate_speech_features(13);
    float salience = audio_cortex_get_speech_salience(cortex, features.data(), features.size());

    EXPECT_GE(salience, 0.0f) << "Salience should be >= 0";
    EXPECT_LE(salience, 1.0f) << "Salience should be <= 1";
    EXPECT_GT(salience, 0.1f) << "Speech-like features should have some salience";
}

TEST_F(AudioCortexBidirectionalTest, GetSpeechSalienceRangeCheck) {
    // Test with various feature patterns
    for (int test = 0; test < 10; test++) {
        std::vector<float> features(13);
        for (uint32_t i = 0; i < features.size(); i++) {
            features[i] = static_cast<float>(test) / 10.0f;
        }

        float salience = audio_cortex_get_speech_salience(cortex, features.data(), features.size());
        EXPECT_GE(salience, 0.0f) << "Salience should be >= 0 for test " << test;
        EXPECT_LE(salience, 1.0f) << "Salience should be <= 1 for test " << test;
    }
}

// =============================================================================
// SPEECH MODE ACTIVATION TESTS
// =============================================================================

TEST_F(AudioCortexBidirectionalTest, ActivateSpeechModeNullCortex) {
    // Should not crash
    audio_cortex_activate_speech_mode(nullptr);
    SUCCEED() << "Null cortex handled gracefully";
}

TEST_F(AudioCortexBidirectionalTest, ActivateSpeechModeValid) {
    // Should not crash
    audio_cortex_activate_speech_mode(cortex);
    SUCCEED() << "Speech mode activation completed";
}

TEST_F(AudioCortexBidirectionalTest, ActivateSpeechModeMultipleTimes) {
    // Should be idempotent
    audio_cortex_activate_speech_mode(cortex);
    audio_cortex_activate_speech_mode(cortex);
    audio_cortex_activate_speech_mode(cortex);
    SUCCEED() << "Multiple activations handled gracefully";
}

// =============================================================================
// INTEGRATION TESTS
// =============================================================================

TEST_F(AudioCortexBidirectionalTest, SpeechDetectionWorkflow) {
    // Simulate speech detection workflow
    std::vector<float> speech_features = generate_speech_features(13);

    // 1. Detect speech salience
    float salience = audio_cortex_get_speech_salience(cortex, speech_features.data(), speech_features.size());
    EXPECT_GE(salience, 0.0f);

    // 2. If speech detected (salience > threshold), activate speech mode
    if (salience > 0.5f) {
        audio_cortex_activate_speech_mode(cortex);
        SUCCEED() << "Speech mode activated for high salience";
    }
}

// Note: main() provided by GTest::Main in CMake configuration
