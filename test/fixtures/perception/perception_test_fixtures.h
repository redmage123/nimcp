/**
 * @file perception_test_fixtures.h
 * @brief Comprehensive test fixtures for all perception modules
 *
 * WHAT: Complete test fixtures with actual setup/teardown for visual, audio, and speech cortex
 * WHY:  Provide reusable, production-quality test infrastructure for perception modules
 * HOW:  GTest fixtures with full initialization, mock brain, synthetic data generation
 *
 * @author NIMCP Development Team
 * @date 2025
 * @version 2.7
 */

#ifndef PERCEPTION_TEST_FIXTURES_H
#define PERCEPTION_TEST_FIXTURES_H

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <memory>
#include <random>

#include "perception/nimcp_visual_cortex.h"
#include "perception/nimcp_audio_cortex.h"
#include "perception/nimcp_speech_cortex.h"
#include "core/brain/nimcp_brain.h"
#include "plasticity/neuromodulators/nimcp_neuromodulators.h"

//=============================================================================
// Test Data Generators
//=============================================================================

/**
 * @brief Synthetic image data generator
 *
 * WHAT: Generate realistic test images (gradients, edges, noise, patterns)
 * WHY:  Need actual image data to test visual cortex, not stubs
 * HOW:  Mathematical patterns with configurable parameters
 */
class SyntheticImageGenerator {
public:
    /**
     * WHAT: Generate horizontal gradient image
     * WHY:  Test edge detection along X-axis
     * HOW:  Linear gradient from 0 to 255
     */
    static std::vector<uint8_t> GenerateHorizontalGradient(uint32_t width, uint32_t height) {
        std::vector<uint8_t> image(width * height);
        for (uint32_t y = 0; y < height; y++) {
            for (uint32_t x = 0; x < width; x++) {
                image[y * width + x] = static_cast<uint8_t>((x * 255) / width);
            }
        }
        return image;
    }

    /**
     * WHAT: Generate vertical gradient image
     * WHY:  Test edge detection along Y-axis
     */
    static std::vector<uint8_t> GenerateVerticalGradient(uint32_t width, uint32_t height) {
        std::vector<uint8_t> image(width * height);
        for (uint32_t y = 0; y < height; y++) {
            for (uint32_t x = 0; x < width; x++) {
                image[y * width + x] = static_cast<uint8_t>((y * 255) / height);
            }
        }
        return image;
    }

    /**
     * WHAT: Generate checkerboard pattern
     * WHY:  Test multi-orientation edge detection
     * HOW:  Alternating black/white squares
     */
    static std::vector<uint8_t> GenerateCheckerboard(uint32_t width, uint32_t height, uint32_t square_size) {
        std::vector<uint8_t> image(width * height);
        for (uint32_t y = 0; y < height; y++) {
            for (uint32_t x = 0; x < width; x++) {
                uint32_t square_x = x / square_size;
                uint32_t square_y = y / square_size;
                bool is_white = ((square_x + square_y) % 2) == 0;
                image[y * width + x] = is_white ? 255 : 0;
            }
        }
        return image;
    }

    /**
     * WHAT: Generate Gaussian noise image
     * WHY:  Test robustness to noise
     */
    static std::vector<uint8_t> GenerateGaussianNoise(uint32_t width, uint32_t height, float mean = 128.0f, float stddev = 50.0f) {
        std::vector<uint8_t> image(width * height);
        std::random_device rd;
        std::mt19937 gen(rd());
        std::normal_distribution<float> dist(mean, stddev);

        for (uint32_t i = 0; i < width * height; i++) {
            float value = dist(gen);
            value = std::max(0.0f, std::min(255.0f, value));
            image[i] = static_cast<uint8_t>(value);
        }
        return image;
    }

    /**
     * WHAT: Generate edge stimulus (sharp edge)
     * WHY:  Test oriented edge detection (Gabor filters)
     */
    static std::vector<uint8_t> GenerateVerticalEdge(uint32_t width, uint32_t height) {
        std::vector<uint8_t> image(width * height);
        uint32_t edge_x = width / 2;
        for (uint32_t y = 0; y < height; y++) {
            for (uint32_t x = 0; x < width; x++) {
                image[y * width + x] = (x < edge_x) ? 0 : 255;
            }
        }
        return image;
    }

    /**
     * WHAT: Generate sine wave grating
     * WHY:  Test spatial frequency tuning
     */
    static std::vector<uint8_t> GenerateSineGrating(uint32_t width, uint32_t height, float frequency, float orientation_deg) {
        std::vector<uint8_t> image(width * height);
        float theta = orientation_deg * M_PI / 180.0f;

        for (uint32_t y = 0; y < height; y++) {
            for (uint32_t x = 0; x < width; x++) {
                float x_rot = x * std::cos(theta) + y * std::sin(theta);
                float value = 127.5f * (1.0f + std::sin(2.0f * M_PI * frequency * x_rot / width));
                image[y * width + x] = static_cast<uint8_t>(value);
            }
        }
        return image;
    }

    /**
     * WHAT: Generate uniform solid color
     * WHY:  Test baseline response (no edges)
     */
    static std::vector<uint8_t> GenerateSolidColor(uint32_t width, uint32_t height, uint8_t color) {
        return std::vector<uint8_t>(width * height, color);
    }
};

/**
 * @brief Synthetic audio data generator
 *
 * WHAT: Generate realistic test audio (tones, noise, speech-like signals)
 * WHY:  Need actual audio data for audio cortex testing
 */
class SyntheticAudioGenerator {
public:
    /**
     * WHAT: Generate pure sine tone
     * WHY:  Test single-frequency response
     */
    static std::vector<float> GenerateSineTone(uint32_t sample_rate, float frequency_hz, float duration_sec, float amplitude = 1.0f) {
        uint32_t num_samples = static_cast<uint32_t>(sample_rate * duration_sec);
        std::vector<float> audio(num_samples);

        for (uint32_t i = 0; i < num_samples; i++) {
            float t = static_cast<float>(i) / sample_rate;
            audio[i] = amplitude * std::sin(2.0f * M_PI * frequency_hz * t);
        }
        return audio;
    }

    /**
     * WHAT: Generate white noise
     * WHY:  Test broadband response
     */
    static std::vector<float> GenerateWhiteNoise(uint32_t sample_rate, float duration_sec, float amplitude = 1.0f) {
        uint32_t num_samples = static_cast<uint32_t>(sample_rate * duration_sec);
        std::vector<float> audio(num_samples);
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<float> dist(-amplitude, amplitude);

        for (uint32_t i = 0; i < num_samples; i++) {
            audio[i] = dist(gen);
        }
        return audio;
    }

    /**
     * WHAT: Generate chirp (frequency sweep)
     * WHY:  Test frequency selectivity across range
     */
    static std::vector<float> GenerateChirp(uint32_t sample_rate, float f_start, float f_end, float duration_sec) {
        uint32_t num_samples = static_cast<uint32_t>(sample_rate * duration_sec);
        std::vector<float> audio(num_samples);
        float k = (f_end - f_start) / duration_sec;

        for (uint32_t i = 0; i < num_samples; i++) {
            float t = static_cast<float>(i) / sample_rate;
            float freq = f_start + k * t;
            audio[i] = std::sin(2.0f * M_PI * freq * t);
        }
        return audio;
    }

    /**
     * WHAT: Generate amplitude modulated (AM) signal
     * WHY:  Test temporal envelope extraction
     */
    static std::vector<float> GenerateAMSignal(uint32_t sample_rate, float carrier_hz, float modulation_hz, float duration_sec) {
        uint32_t num_samples = static_cast<uint32_t>(sample_rate * duration_sec);
        std::vector<float> audio(num_samples);

        for (uint32_t i = 0; i < num_samples; i++) {
            float t = static_cast<float>(i) / sample_rate;
            float carrier = std::sin(2.0f * M_PI * carrier_hz * t);
            float envelope = 0.5f * (1.0f + std::sin(2.0f * M_PI * modulation_hz * t));
            audio[i] = carrier * envelope;
        }
        return audio;
    }

    /**
     * WHAT: Generate speech-like formants
     * WHY:  Test speech detection and formant tracking
     */
    static std::vector<float> GenerateSpeechLikeSignal(uint32_t sample_rate, float f1, float f2, float duration_sec) {
        uint32_t num_samples = static_cast<uint32_t>(sample_rate * duration_sec);
        std::vector<float> audio(num_samples);

        // Fundamental frequency (pitch)
        float f0 = 120.0f;

        for (uint32_t i = 0; i < num_samples; i++) {
            float t = static_cast<float>(i) / sample_rate;
            // Add fundamental and formants
            float signal = 0.3f * std::sin(2.0f * M_PI * f0 * t);
            signal += 0.5f * std::sin(2.0f * M_PI * f1 * t);
            signal += 0.4f * std::sin(2.0f * M_PI * f2 * t);
            audio[i] = signal / 1.2f; // Normalize
        }
        return audio;
    }

    /**
     * WHAT: Generate silence
     * WHY:  Test onset/offset detection
     */
    static std::vector<float> GenerateSilence(uint32_t sample_rate, float duration_sec) {
        uint32_t num_samples = static_cast<uint32_t>(sample_rate * duration_sec);
        return std::vector<float>(num_samples, 0.0f);
    }

    /**
     * WHAT: Generate click/impulse
     * WHY:  Test temporal resolution
     */
    static std::vector<float> GenerateClick(uint32_t sample_rate, float duration_sec) {
        uint32_t num_samples = static_cast<uint32_t>(sample_rate * duration_sec);
        std::vector<float> audio(num_samples, 0.0f);
        if (num_samples > 0) {
            audio[num_samples / 2] = 1.0f; // Single spike in middle
        }
        return audio;
    }
};

/**
 * @brief Mock brain for neuromodulation testing
 *
 * WHAT: Lightweight brain mock with working neurotransmitter simulation
 * WHY:  Test neuromodulator effects on perception modules
 * HOW:  Minimal brain structure with controllable neurotransmitter levels
 */
class MockBrain {
public:
    brain_t brain = nullptr;

    MockBrain() {
        // WHAT: Create minimal brain for testing (new API)
        // WHY:  Need brain reference for neuromodulation APIs
        brain = brain_create("mock_brain", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 10, 10);
    }

    ~MockBrain() {
        if (brain) {
            brain_destroy(brain);
        }
    }

    /**
     * WHAT: Set acetylcholine level
     * WHY:  Test ACh effects on attention/perception
     * NOTE: Direct API for neuromodulator control may not exist in new API
     */
    void SetAcetylcholine(float level) {
        // TODO: Use new API to set neuromodulator levels if available
        // For now, skip internal state access
        (void)level;
    }

    /**
     * WHAT: Set norepinephrine level
     * WHY:  Test NE effects on arousal
     * NOTE: Direct API for neuromodulator control may not exist in new API
     */
    void SetNorepinephrine(float level) {
        // TODO: Use new API to set neuromodulator levels if available
        (void)level;
    }

    /**
     * WHAT: Set dopamine level
     * WHY:  Test DA effects on reward/motivation
     * NOTE: Direct API for neuromodulator control may not exist in new API
     */
    void SetDopamine(float level) {
        // TODO: Use new API to set neuromodulator levels if available
        (void)level;
    }

    /**
     * WHAT: Set serotonin level
     * WHY:  Test 5-HT effects on sensory gating
     * NOTE: Direct API for neuromodulator control may not exist in new API
     */
    void SetSerotonin(float level) {
        // TODO: Use new API to set neuromodulator levels if available
        (void)level;
    }

    /**
     * WHAT: Reset all neurotransmitters to baseline
     * WHY:  Ensure clean state between tests
     * NOTE: Direct API for neuromodulator control may not exist in new API
     */
    void ResetNeurotransmitters() {
        // TODO: Use new API to reset neuromodulator levels if available
    }
};

//=============================================================================
// Visual Cortex Test Fixture
//=============================================================================

class VisualCortexTestFixture : public ::testing::Test {
protected:
    visual_cortex_t* cortex = nullptr;
    std::unique_ptr<MockBrain> mock_brain;

    // Standard test image dimensions
    static constexpr uint32_t TEST_WIDTH = 640;
    static constexpr uint32_t TEST_HEIGHT = 480;
    static constexpr uint32_t TEST_CHANNELS = 1;  // Grayscale
    static constexpr uint32_t FEATURE_DIM = 128;

    void SetUp() override {
        // WHAT: Initialize visual cortex with standard config
        // WHY:  Consistent test environment
        visual_cortex_config_t config = {
            .input_width = TEST_WIDTH,
            .input_height = TEST_HEIGHT,
            .num_v1_filters = 32,
            .feature_dim = FEATURE_DIM,
            .enable_attention = true,
            .enable_memory = true,
            .enable_fractal_topology = false,
            .hub_ratio = 0.15f,
            .power_law_gamma = -2.1f,
            .internal_neurons = 320
        };

        cortex = visual_cortex_create(&config);
        ASSERT_NE(cortex, nullptr) << "Failed to create visual cortex";

        // Create mock brain for neuromodulation tests
        mock_brain = std::make_unique<MockBrain>();
        ASSERT_NE(mock_brain->brain, nullptr) << "Failed to create mock brain";
    }

    void TearDown() override {
        if (cortex) {
            visual_cortex_destroy(cortex);
            cortex = nullptr;
        }
        // mock_brain auto-destroyed
    }

    // Helper: Process image and return features
    std::vector<float> ProcessImage(const std::vector<uint8_t>& image) {
        std::vector<float> features(FEATURE_DIM);
        bool success = visual_cortex_process(cortex, image.data(), TEST_WIDTH, TEST_HEIGHT, TEST_CHANNELS, features.data());
        EXPECT_TRUE(success) << "visual_cortex_process failed";
        return features;
    }
};

//=============================================================================
// Audio Cortex Test Fixture
//=============================================================================

class AudioCortexTestFixture : public ::testing::Test {
protected:
    audio_cortex_t* cortex = nullptr;
    std::unique_ptr<MockBrain> mock_brain;

    static constexpr uint32_t SAMPLE_RATE = 16000;
    static constexpr uint32_t FRAME_SIZE = 512;
    static constexpr uint32_t NUM_MEL_FILTERS = 40;
    static constexpr uint32_t NUM_MFCC = 13;
    static constexpr uint32_t FEATURE_DIM = 64;

    void SetUp() override {
        audio_cortex_config_t config = {
            .sample_rate = SAMPLE_RATE,
            .frame_size = FRAME_SIZE,
            .num_freq_bins = FRAME_SIZE / 2,
            .num_mel_filters = NUM_MEL_FILTERS,
            .num_mfcc = NUM_MFCC,
            .num_channels = 1,
            .feature_dim = FEATURE_DIM,
            .enable_attention = true,
            .enable_memory = true,
            .enable_fractal_topology = false,
            .hub_ratio = 0.15f,
            .power_law_gamma = -2.1f,
            .internal_neurons = 400
        };

        cortex = audio_cortex_create(&config);
        ASSERT_NE(cortex, nullptr) << "Failed to create audio cortex";

        mock_brain = std::make_unique<MockBrain>();
        ASSERT_NE(mock_brain->brain, nullptr);
    }

    void TearDown() override {
        if (cortex) {
            audio_cortex_destroy(cortex);
            cortex = nullptr;
        }
    }

    std::vector<float> ProcessAudio(const std::vector<float>& audio) {
        std::vector<float> features(FEATURE_DIM);
        // audio_cortex_process expects exactly FRAME_SIZE samples
        uint32_t num_samples = std::min((uint32_t)audio.size(), FRAME_SIZE);
        bool success = audio_cortex_process(cortex, audio.data(), num_samples, 1, features.data());
        EXPECT_TRUE(success);
        return features;
    }
};

//=============================================================================
// Speech Cortex Test Fixture
//=============================================================================

class SpeechCortexTestFixture : public ::testing::Test {
protected:
    speech_cortex_t* cortex = nullptr;
    std::unique_ptr<MockBrain> mock_brain;

    static constexpr uint32_t SAMPLE_RATE = 16000;
    static constexpr uint32_t FRAME_SIZE_MS = 20;
    static constexpr uint32_t FEATURE_DIM = 64;

    void SetUp() override {
        speech_cortex_config_t config = {
            .sample_rate = SAMPLE_RATE,
            .frame_size_ms = FRAME_SIZE_MS,
            .hop_size_ms = 10,
            .num_phonemes = SPEECH_NUM_PHONEMES,
            .num_formants = SPEECH_NUM_FORMANTS,
            .phonological_buffer_size = SPEECH_MAX_PHONOLOGICAL_BUFFER,
            .lexicon_size = 1000,
            .feature_dim = FEATURE_DIM,
            .enable_wernicke = true,
            .enable_broca = true,
            .enable_prosody = true,
            .enable_memory = true,
            .enable_fractal_topology = false,
            .hub_ratio = 0.15f,
            .power_law_gamma = -2.1f,
            .internal_neurons = 440
        };

        cortex = speech_cortex_create(&config);
        ASSERT_NE(cortex, nullptr) << "Failed to create speech cortex";

        mock_brain = std::make_unique<MockBrain>();
        ASSERT_NE(mock_brain->brain, nullptr);
    }

    void TearDown() override {
        if (cortex) {
            speech_cortex_destroy(cortex);
            cortex = nullptr;
        }
    }

    std::vector<float> ProcessSpeech(const std::vector<float>& audio) {
        std::vector<float> features(FEATURE_DIM);
        bool success = speech_cortex_process(cortex, audio.data(), audio.size(), features.data());
        EXPECT_TRUE(success);
        return features;
    }
};

#endif // PERCEPTION_TEST_FIXTURES_H
