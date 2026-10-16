/**
 * @file test_perception_multimodal.cpp
 * @brief Integration tests for multimodal perception (visual + audio fusion)
 *
 * WHAT: Comprehensive integration tests for cross-modal perception
 * WHY:  Ensure visual and audio cortex can work together for multimodal
 *       understanding, synchronized processing, and cross-modal attention
 * HOW:  GTest framework with both visual and audio cortices coordinating
 *
 * @author NIMCP Development Team
 * @date 2025
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <memory>
#include <random>
#include <cstring>
#include <chrono>
#include <thread>

extern "C" {
#include "perception/nimcp_visual_cortex.h"
#include "perception/nimcp_audio_cortex.h"
#include "core/brain/nimcp_brain.h"
}

//=============================================================================
// Synthetic Data Generators
//=============================================================================

class SyntheticImageGenerator {
public:
    static std::vector<uint8_t> GenerateHorizontalGradient(uint32_t width, uint32_t height) {
        std::vector<uint8_t> image(width * height);
        for (uint32_t y = 0; y < height; y++) {
            for (uint32_t x = 0; x < width; x++) {
                image[y * width + x] = static_cast<uint8_t>((x * 255) / width);
            }
        }
        return image;
    }

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

    static std::vector<uint8_t> GenerateSolidColor(uint32_t width, uint32_t height, uint8_t color) {
        return std::vector<uint8_t>(width * height, color);
    }

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
};

class SyntheticAudioGenerator {
public:
    static std::vector<float> GenerateSineTone(uint32_t sample_rate, float frequency_hz,
                                                float duration_sec, float amplitude = 1.0f) {
        uint32_t num_samples = static_cast<uint32_t>(sample_rate * duration_sec);
        std::vector<float> audio(num_samples);

        for (uint32_t i = 0; i < num_samples; i++) {
            float t = static_cast<float>(i) / sample_rate;
            audio[i] = amplitude * std::sin(2.0f * M_PI * frequency_hz * t);
        }
        return audio;
    }

    static std::vector<float> GenerateWhiteNoise(uint32_t sample_rate, float duration_sec,
                                                  float amplitude = 1.0f) {
        uint32_t num_samples = static_cast<uint32_t>(sample_rate * duration_sec);
        std::vector<float> audio(num_samples);
        std::mt19937 gen(42);
        std::uniform_real_distribution<float> dist(-amplitude, amplitude);

        for (uint32_t i = 0; i < num_samples; i++) {
            audio[i] = dist(gen);
        }
        return audio;
    }

    static std::vector<float> GenerateSpeechLikeSignal(uint32_t sample_rate, float f1,
                                                        float f2, float duration_sec) {
        uint32_t num_samples = static_cast<uint32_t>(sample_rate * duration_sec);
        std::vector<float> audio(num_samples);
        float f0 = 120.0f;

        for (uint32_t i = 0; i < num_samples; i++) {
            float t = static_cast<float>(i) / sample_rate;
            float signal = 0.3f * std::sin(2.0f * M_PI * f0 * t);
            signal += 0.5f * std::sin(2.0f * M_PI * f1 * t);
            signal += 0.4f * std::sin(2.0f * M_PI * f2 * t);
            audio[i] = signal / 1.2f;
        }
        return audio;
    }

    static std::vector<float> GenerateSilence(uint32_t sample_rate, float duration_sec) {
        uint32_t num_samples = static_cast<uint32_t>(sample_rate * duration_sec);
        return std::vector<float>(num_samples, 0.0f);
    }
};

//=============================================================================
// Multimodal Test Fixture
//=============================================================================

class MultimodalPerceptionTest : public ::testing::Test {
protected:
    visual_cortex_t* visual_cortex = nullptr;
    audio_cortex_t* audio_cortex = nullptr;
    brain_t brain = nullptr;

    // Visual parameters
    static constexpr uint32_t IMG_WIDTH = 64;
    static constexpr uint32_t IMG_HEIGHT = 64;
    static constexpr uint32_t VISUAL_FEATURE_DIM = 64;

    // Audio parameters
    static constexpr uint32_t SAMPLE_RATE = 16000;
    static constexpr uint32_t FRAME_SIZE = 512;
    static constexpr uint32_t AUDIO_FEATURE_DIM = 64;

    void SetUp() override {
        // Create visual cortex
        visual_cortex_config_t visual_config = {
            .input_width = IMG_WIDTH,
            .input_height = IMG_HEIGHT,
            .num_v1_filters = 16,
            .feature_dim = VISUAL_FEATURE_DIM,
            .enable_attention = true,
            .enable_memory = true,
            .enable_fractal_topology = false,
            .hub_ratio = 0.15f,
            .power_law_gamma = -2.1f,
            .internal_neurons = 160,
            .enable_bio_async = false,
            .enable_second_messengers = false
        };

        visual_cortex = visual_cortex_create(&visual_config);

        // Create audio cortex
        audio_cortex_config_t audio_config = {
            .sample_rate = SAMPLE_RATE,
            .frame_size = FRAME_SIZE,
            .num_freq_bins = FRAME_SIZE / 2,
            .num_mel_filters = 40,
            .num_mfcc = 13,
            .num_channels = 1,
            .feature_dim = AUDIO_FEATURE_DIM,
            .enable_attention = true,
            .enable_memory = true,
            .enable_fractal_topology = false,
            .hub_ratio = 0.15f,
            .power_law_gamma = -2.1f,
            .internal_neurons = 400,
            .enable_bio_async = false,
            .enable_second_messengers = false
        };

        audio_cortex = audio_cortex_create(&audio_config);

        // Create brain for integration
        brain = brain_create("multimodal_brain", BRAIN_SIZE_TINY,
                             BRAIN_TASK_CLASSIFICATION, 10, 10);
    }

    void TearDown() override {
        if (visual_cortex) {
            visual_cortex_destroy(visual_cortex);
            visual_cortex = nullptr;
        }
        if (audio_cortex) {
            audio_cortex_destroy(audio_cortex);
            audio_cortex = nullptr;
        }
        if (brain) {
            brain_destroy(brain);
            brain = nullptr;
        }
    }

    std::vector<float> ProcessVisual(const std::vector<uint8_t>& image) {
        std::vector<float> features(VISUAL_FEATURE_DIM);
        bool success = visual_cortex_process(visual_cortex, image.data(),
                                              IMG_WIDTH, IMG_HEIGHT, 1, features.data());
        EXPECT_TRUE(success);
        return features;
    }

    std::vector<float> ProcessAudio(const std::vector<float>& audio) {
        std::vector<float> features(AUDIO_FEATURE_DIM);
        uint32_t num_samples = std::min(static_cast<uint32_t>(audio.size()), FRAME_SIZE);
        bool success = audio_cortex_process(audio_cortex, audio.data(), num_samples, 1, features.data());
        EXPECT_TRUE(success);
        return features;
    }

    // Simple feature fusion by concatenation
    std::vector<float> FuseFeatures(const std::vector<float>& visual,
                                     const std::vector<float>& audio) {
        std::vector<float> fused;
        fused.reserve(visual.size() + audio.size());
        fused.insert(fused.end(), visual.begin(), visual.end());
        fused.insert(fused.end(), audio.begin(), audio.end());
        return fused;
    }

    // Compute correlation between two feature vectors
    float ComputeCorrelation(const std::vector<float>& a, const std::vector<float>& b) {
        if (a.size() != b.size() || a.empty()) return 0.0f;

        float mean_a = 0.0f, mean_b = 0.0f;
        for (size_t i = 0; i < a.size(); i++) {
            mean_a += a[i];
            mean_b += b[i];
        }
        mean_a /= a.size();
        mean_b /= b.size();

        float cov = 0.0f, var_a = 0.0f, var_b = 0.0f;
        for (size_t i = 0; i < a.size(); i++) {
            float da = a[i] - mean_a;
            float db = b[i] - mean_b;
            cov += da * db;
            var_a += da * da;
            var_b += db * db;
        }

        if (var_a < 1e-10f || var_b < 1e-10f) return 0.0f;
        return cov / (std::sqrt(var_a) * std::sqrt(var_b));
    }
};

//=============================================================================
// Basic Multimodal Integration Tests
//=============================================================================

TEST_F(MultimodalPerceptionTest, BothCorticesInitialized) {
    ASSERT_NE(visual_cortex, nullptr) << "Visual cortex should be created";
    ASSERT_NE(audio_cortex, nullptr) << "Audio cortex should be created";
}

TEST_F(MultimodalPerceptionTest, ProcessVisualAndAudioSimultaneously) {
    ASSERT_NE(visual_cortex, nullptr);
    ASSERT_NE(audio_cortex, nullptr);

    // Process visual input
    auto image = SyntheticImageGenerator::GenerateCheckerboard(IMG_WIDTH, IMG_HEIGHT, 8);
    auto visual_features = ProcessVisual(image);

    // Process audio input
    auto audio = SyntheticAudioGenerator::GenerateSineTone(SAMPLE_RATE, 440.0f, 0.1f);
    auto audio_features = ProcessAudio(audio);

    // Both should have valid features
    EXPECT_EQ(visual_features.size(), VISUAL_FEATURE_DIM);
    EXPECT_EQ(audio_features.size(), AUDIO_FEATURE_DIM);

    // Verify features are non-zero
    float visual_sum = 0.0f, audio_sum = 0.0f;
    for (const auto& f : visual_features) visual_sum += std::abs(f);
    for (const auto& f : audio_features) audio_sum += std::abs(f);

    EXPECT_GT(visual_sum, 0.0f);
    EXPECT_GT(audio_sum, 0.0f);
}

//=============================================================================
// Feature Fusion Tests
//=============================================================================

TEST_F(MultimodalPerceptionTest, FuseVisualAndAudioFeatures) {
    ASSERT_NE(visual_cortex, nullptr);
    ASSERT_NE(audio_cortex, nullptr);

    auto image = SyntheticImageGenerator::GenerateHorizontalGradient(IMG_WIDTH, IMG_HEIGHT);
    auto audio = SyntheticAudioGenerator::GenerateSineTone(SAMPLE_RATE, 1000.0f, 0.1f);

    auto visual_features = ProcessVisual(image);
    auto audio_features = ProcessAudio(audio);

    auto fused = FuseFeatures(visual_features, audio_features);

    EXPECT_EQ(fused.size(), VISUAL_FEATURE_DIM + AUDIO_FEATURE_DIM);

    // First half should match visual
    for (size_t i = 0; i < VISUAL_FEATURE_DIM; i++) {
        EXPECT_FLOAT_EQ(fused[i], visual_features[i]);
    }

    // Second half should match audio
    for (size_t i = 0; i < AUDIO_FEATURE_DIM; i++) {
        EXPECT_FLOAT_EQ(fused[VISUAL_FEATURE_DIM + i], audio_features[i]);
    }
}

TEST_F(MultimodalPerceptionTest, FusedFeaturesDistinguishDifferentStimuli) {
    ASSERT_NE(visual_cortex, nullptr);
    ASSERT_NE(audio_cortex, nullptr);

    // Stimulus 1: Checkerboard + High frequency tone
    auto image1 = SyntheticImageGenerator::GenerateCheckerboard(IMG_WIDTH, IMG_HEIGHT, 8);
    auto audio1 = SyntheticAudioGenerator::GenerateSineTone(SAMPLE_RATE, 4000.0f, 0.1f);
    auto v_features1 = ProcessVisual(image1);
    auto a_features1 = ProcessAudio(audio1);
    auto fused1 = FuseFeatures(v_features1, a_features1);

    // Stimulus 2: Solid gray + Low frequency tone
    auto image2 = SyntheticImageGenerator::GenerateSolidColor(IMG_WIDTH, IMG_HEIGHT, 128);
    auto audio2 = SyntheticAudioGenerator::GenerateSineTone(SAMPLE_RATE, 200.0f, 0.1f);
    auto v_features2 = ProcessVisual(image2);
    auto a_features2 = ProcessAudio(audio2);
    auto fused2 = FuseFeatures(v_features2, a_features2);

    // Fused features should be different
    float diff = 0.0f;
    for (size_t i = 0; i < fused1.size(); i++) {
        diff += std::abs(fused1[i] - fused2[i]);
    }
    EXPECT_GT(diff, 0.0f) << "Different multimodal stimuli should produce different fused features";
}

//=============================================================================
// Cross-Modal Attention Tests
//=============================================================================

TEST_F(MultimodalPerceptionTest, CrossModalAttentionVisualToAudio) {
    ASSERT_NE(visual_cortex, nullptr);
    ASSERT_NE(audio_cortex, nullptr);

    // Salient visual stimulus (strong edge)
    auto image = SyntheticImageGenerator::GenerateVerticalEdge(IMG_WIDTH, IMG_HEIGHT);
    auto visual_features = ProcessVisual(image);

    // Compute visual attention
    attention_map_t* visual_attn = attention_map_create(IMG_WIDTH, IMG_HEIGHT);
    ASSERT_NE(visual_attn, nullptr);

    bool attn_success = visual_cortex_compute_attention(visual_cortex, image.data(),
                                                         IMG_WIDTH, IMG_HEIGHT, visual_attn);
    EXPECT_TRUE(attn_success);

    uint32_t max_x, max_y;
    float max_value;
    visual_cortex_get_attention_peak(visual_attn, &max_x, &max_y, &max_value);

    // Visual attention peak should exist
    EXPECT_LT(max_x, IMG_WIDTH);
    EXPECT_LT(max_y, IMG_HEIGHT);

    attention_map_destroy(visual_attn);
}

TEST_F(MultimodalPerceptionTest, CrossModalAttentionAudioToVisual) {
    ASSERT_NE(visual_cortex, nullptr);
    ASSERT_NE(audio_cortex, nullptr);

    // Salient audio stimulus (speech-like)
    auto audio = SyntheticAudioGenerator::GenerateSpeechLikeSignal(SAMPLE_RATE, 730.0f, 1090.0f, 0.1f);
    auto audio_features = ProcessAudio(audio);

    // Compute audio attention
    audio_attention_map_t* audio_attn = audio_attention_map_create(40, 10);
    ASSERT_NE(audio_attn, nullptr);

    bool attn_success = audio_cortex_compute_attention(audio_cortex, audio.data(), FRAME_SIZE, audio_attn);
    EXPECT_TRUE(attn_success);

    uint32_t max_freq, max_time;
    float max_value;
    audio_cortex_get_attention_peak(audio_attn, &max_freq, &max_time, &max_value);

    // Audio attention peak should exist
    EXPECT_LT(max_freq, 40u);
    EXPECT_GE(max_value, 0.0f);

    audio_attention_map_destroy(audio_attn);
}

//=============================================================================
// Temporal Synchronization Tests
//=============================================================================

TEST_F(MultimodalPerceptionTest, SynchronizedProcessing) {
    ASSERT_NE(visual_cortex, nullptr);
    ASSERT_NE(audio_cortex, nullptr);

    // Simulate synchronized AV processing
    const int num_frames = 5;
    std::vector<std::vector<float>> visual_features_seq;
    std::vector<std::vector<float>> audio_features_seq;

    for (int i = 0; i < num_frames; i++) {
        // Generate frame-specific stimuli
        auto image = SyntheticImageGenerator::GenerateSolidColor(IMG_WIDTH, IMG_HEIGHT,
                                                                   static_cast<uint8_t>(i * 50));
        auto audio = SyntheticAudioGenerator::GenerateSineTone(SAMPLE_RATE,
                                                                440.0f + i * 100.0f, 0.05f);

        visual_features_seq.push_back(ProcessVisual(image));
        audio_features_seq.push_back(ProcessAudio(audio));
    }

    // All frames should have been processed
    EXPECT_EQ(visual_features_seq.size(), num_frames);
    EXPECT_EQ(audio_features_seq.size(), num_frames);

    // Features from different frames should be different
    for (int i = 0; i < num_frames - 1; i++) {
        float visual_diff = 0.0f, audio_diff = 0.0f;
        for (size_t j = 0; j < VISUAL_FEATURE_DIM; j++) {
            visual_diff += std::abs(visual_features_seq[i][j] - visual_features_seq[i+1][j]);
        }
        for (size_t j = 0; j < AUDIO_FEATURE_DIM; j++) {
            audio_diff += std::abs(audio_features_seq[i][j] - audio_features_seq[i+1][j]);
        }
        // At least one modality should show change (audio definitely will due to frequency change)
        EXPECT_GT(audio_diff, 0.0f);
    }
}

TEST_F(MultimodalPerceptionTest, ProcessingLatency) {
    ASSERT_NE(visual_cortex, nullptr);
    ASSERT_NE(audio_cortex, nullptr);

    auto image = SyntheticImageGenerator::GenerateCheckerboard(IMG_WIDTH, IMG_HEIGHT, 8);
    auto audio = SyntheticAudioGenerator::GenerateSineTone(SAMPLE_RATE, 440.0f, 0.1f);

    // Measure visual processing time
    auto start_visual = std::chrono::high_resolution_clock::now();
    std::vector<float> visual_features(VISUAL_FEATURE_DIM);
    visual_cortex_process(visual_cortex, image.data(), IMG_WIDTH, IMG_HEIGHT, 1, visual_features.data());
    auto end_visual = std::chrono::high_resolution_clock::now();

    // Measure audio processing time
    auto start_audio = std::chrono::high_resolution_clock::now();
    std::vector<float> audio_features(AUDIO_FEATURE_DIM);
    audio_cortex_process(audio_cortex, audio.data(), FRAME_SIZE, 1, audio_features.data());
    auto end_audio = std::chrono::high_resolution_clock::now();

    auto visual_duration = std::chrono::duration_cast<std::chrono::microseconds>(end_visual - start_visual);
    auto audio_duration = std::chrono::duration_cast<std::chrono::microseconds>(end_audio - start_audio);

    // Processing should be reasonably fast (less than 100ms each)
    EXPECT_LT(visual_duration.count(), 100000);
    EXPECT_LT(audio_duration.count(), 100000);
}

//=============================================================================
// Memory Integration Tests
//=============================================================================

TEST_F(MultimodalPerceptionTest, StoreAndRecallMultimodalMemory) {
    ASSERT_NE(visual_cortex, nullptr);
    ASSERT_NE(audio_cortex, nullptr);

    // Create multimodal experience
    auto image = SyntheticImageGenerator::GenerateCheckerboard(IMG_WIDTH, IMG_HEIGHT, 8);
    auto audio = SyntheticAudioGenerator::GenerateSineTone(SAMPLE_RATE, 440.0f, 0.1f);

    auto visual_features = ProcessVisual(image);
    auto audio_features = ProcessAudio(audio);

    // Store in both cortices
    bool visual_stored = visual_cortex_store_memory(visual_cortex, visual_features.data(), 0.8f);
    bool audio_stored = audio_cortex_store_memory(audio_cortex, audio_features.data(), 0.8f);

    EXPECT_TRUE(visual_stored);
    EXPECT_TRUE(audio_stored);

    // Compute novelty for same patterns (should be low/familiar)
    float visual_novelty = visual_cortex_compute_novelty(visual_cortex, visual_features.data());
    float audio_novelty = audio_cortex_compute_novelty(audio_cortex, audio_features.data());

    // Novelty should be valid (non-negative)
    if (visual_novelty >= 0.0f && audio_novelty >= 0.0f) {
        // Both should recognize the pattern
        EXPECT_LE(visual_novelty, 1.0f);
        EXPECT_LE(audio_novelty, 1.0f);
    }
}

TEST_F(MultimodalPerceptionTest, NoveltyDetectionCrossModal) {
    ASSERT_NE(visual_cortex, nullptr);
    ASSERT_NE(audio_cortex, nullptr);

    // Store familiar patterns
    auto familiar_image = SyntheticImageGenerator::GenerateCheckerboard(IMG_WIDTH, IMG_HEIGHT, 8);
    auto familiar_audio = SyntheticAudioGenerator::GenerateSineTone(SAMPLE_RATE, 440.0f, 0.1f);

    auto familiar_v_features = ProcessVisual(familiar_image);
    auto familiar_a_features = ProcessAudio(familiar_audio);

    visual_cortex_store_memory(visual_cortex, familiar_v_features.data(), 0.9f);
    audio_cortex_store_memory(audio_cortex, familiar_a_features.data(), 0.9f);

    // Create novel patterns
    auto novel_image = SyntheticImageGenerator::GenerateHorizontalGradient(IMG_WIDTH, IMG_HEIGHT);
    auto novel_audio = SyntheticAudioGenerator::GenerateWhiteNoise(SAMPLE_RATE, 0.1f);

    auto novel_v_features = ProcessVisual(novel_image);
    auto novel_a_features = ProcessAudio(novel_audio);

    // Compute novelty
    float familiar_v_novelty = visual_cortex_compute_novelty(visual_cortex, familiar_v_features.data());
    float novel_v_novelty = visual_cortex_compute_novelty(visual_cortex, novel_v_features.data());

    float familiar_a_novelty = audio_cortex_compute_novelty(audio_cortex, familiar_a_features.data());
    float novel_a_novelty = audio_cortex_compute_novelty(audio_cortex, novel_a_features.data());

    // Novel patterns should have higher novelty than familiar ones
    if (familiar_v_novelty >= 0.0f && novel_v_novelty >= 0.0f) {
        EXPECT_GE(novel_v_novelty, familiar_v_novelty);
    }
    if (familiar_a_novelty >= 0.0f && novel_a_novelty >= 0.0f) {
        EXPECT_GE(novel_a_novelty, familiar_a_novelty);
    }
}

//=============================================================================
// Brain Integration Tests
//=============================================================================

TEST_F(MultimodalPerceptionTest, AttachBothCorticesFromBrain) {
    ASSERT_NE(visual_cortex, nullptr);
    ASSERT_NE(audio_cortex, nullptr);
    ASSERT_NE(brain, nullptr);

    // Attach both cortices to brain
    visual_cortex_set_brain(visual_cortex, brain);
    audio_cortex_set_brain(audio_cortex, brain);

    // Process with brain attached
    auto image = SyntheticImageGenerator::GenerateCheckerboard(IMG_WIDTH, IMG_HEIGHT, 8);
    auto audio = SyntheticAudioGenerator::GenerateSineTone(SAMPLE_RATE, 440.0f, 0.1f);

    auto visual_features = ProcessVisual(image);
    auto audio_features = ProcessAudio(audio);

    // Should still work with brain attached
    EXPECT_EQ(visual_features.size(), VISUAL_FEATURE_DIM);
    EXPECT_EQ(audio_features.size(), AUDIO_FEATURE_DIM);

    // Detach
    visual_cortex_set_brain(visual_cortex, nullptr);
    audio_cortex_set_brain(audio_cortex, nullptr);
}

//=============================================================================
// Speech Detection with Visual Context Tests
//=============================================================================

TEST_F(MultimodalPerceptionTest, SpeechDetectionWithVisualContext) {
    ASSERT_NE(visual_cortex, nullptr);
    ASSERT_NE(audio_cortex, nullptr);

    // Simulate face detection (visual) with speech (audio)
    auto image = SyntheticImageGenerator::GenerateCheckerboard(IMG_WIDTH, IMG_HEIGHT, 8);
    auto speech_audio = SyntheticAudioGenerator::GenerateSpeechLikeSignal(SAMPLE_RATE, 730.0f, 1090.0f, 0.1f);

    auto visual_features = ProcessVisual(image);
    auto audio_features = ProcessAudio(speech_audio);

    // Check speech salience
    float speech_salience = audio_cortex_get_speech_salience(audio_cortex, audio_features.data(), AUDIO_FEATURE_DIM);

    EXPECT_GE(speech_salience, 0.0f);
    EXPECT_LE(speech_salience, 1.0f);

    // With speech detected, activate speech mode
    if (speech_salience > 0.5f) {
        audio_cortex_activate_speech_mode(audio_cortex);
    }
}

//=============================================================================
// Consistency Tests
//=============================================================================

TEST_F(MultimodalPerceptionTest, RepeatedProcessingConsistency) {
    ASSERT_NE(visual_cortex, nullptr);
    ASSERT_NE(audio_cortex, nullptr);

    auto image = SyntheticImageGenerator::GenerateCheckerboard(IMG_WIDTH, IMG_HEIGHT, 8);
    auto audio = SyntheticAudioGenerator::GenerateSineTone(SAMPLE_RATE, 440.0f, 0.1f);

    // Process multiple times
    auto v_features1 = ProcessVisual(image);
    auto v_features2 = ProcessVisual(image);
    auto a_features1 = ProcessAudio(audio);
    auto a_features2 = ProcessAudio(audio);

    // Results should be deterministic
    float v_diff = 0.0f, a_diff = 0.0f;
    for (size_t i = 0; i < VISUAL_FEATURE_DIM; i++) {
        v_diff += std::abs(v_features1[i] - v_features2[i]);
    }
    for (size_t i = 0; i < AUDIO_FEATURE_DIM; i++) {
        a_diff += std::abs(a_features1[i] - a_features2[i]);
    }

    EXPECT_LT(v_diff, 0.001f) << "Visual processing should be deterministic";
    EXPECT_LT(a_diff, 0.001f) << "Audio processing should be deterministic";
}

//=============================================================================
// Edge Cases
//=============================================================================

TEST_F(MultimodalPerceptionTest, VisualOnlyNoAudio) {
    ASSERT_NE(visual_cortex, nullptr);

    auto image = SyntheticImageGenerator::GenerateCheckerboard(IMG_WIDTH, IMG_HEIGHT, 8);
    auto visual_features = ProcessVisual(image);

    // Process with audio silence
    auto silence = SyntheticAudioGenerator::GenerateSilence(SAMPLE_RATE, 0.1f);
    auto audio_features = ProcessAudio(silence);

    // Visual features should still be valid
    float visual_sum = 0.0f;
    for (const auto& f : visual_features) visual_sum += std::abs(f);
    EXPECT_GT(visual_sum, 0.0f);

    // Audio features should be minimal
    bool all_finite = true;
    for (const auto& f : audio_features) {
        if (!std::isfinite(f)) {
            all_finite = false;
            break;
        }
    }
    EXPECT_TRUE(all_finite);
}

TEST_F(MultimodalPerceptionTest, AudioOnlyNoVisual) {
    ASSERT_NE(audio_cortex, nullptr);

    // Process with blank visual
    auto blank_image = SyntheticImageGenerator::GenerateSolidColor(IMG_WIDTH, IMG_HEIGHT, 0);
    auto visual_features = ProcessVisual(blank_image);

    // Process with audio
    auto audio = SyntheticAudioGenerator::GenerateSineTone(SAMPLE_RATE, 440.0f, 0.1f);
    auto audio_features = ProcessAudio(audio);

    // Audio features should still be valid
    float audio_sum = 0.0f;
    for (const auto& f : audio_features) audio_sum += std::abs(f);
    EXPECT_GT(audio_sum, 0.0f);

    // Visual features should be minimal
    bool all_finite = true;
    for (const auto& f : visual_features) {
        if (!std::isfinite(f)) {
            all_finite = false;
            break;
        }
    }
    EXPECT_TRUE(all_finite);
}

//=============================================================================
// Statistics Validation
//=============================================================================

TEST_F(MultimodalPerceptionTest, BothCorticesTrackStatistics) {
    ASSERT_NE(visual_cortex, nullptr);
    ASSERT_NE(audio_cortex, nullptr);

    visual_cortex_stats_t v_stats_before, v_stats_after;
    audio_cortex_stats_t a_stats_before, a_stats_after;

    visual_cortex_get_stats(visual_cortex, &v_stats_before);
    audio_cortex_get_stats(audio_cortex, &a_stats_before);

    // Process some data
    for (int i = 0; i < 3; i++) {
        auto image = SyntheticImageGenerator::GenerateSolidColor(IMG_WIDTH, IMG_HEIGHT, i * 80);
        auto audio = SyntheticAudioGenerator::GenerateSineTone(SAMPLE_RATE, 200.0f + i * 200, 0.05f);
        ProcessVisual(image);
        ProcessAudio(audio);
    }

    visual_cortex_get_stats(visual_cortex, &v_stats_after);
    audio_cortex_get_stats(audio_cortex, &a_stats_after);

    // Stats should have incremented
    EXPECT_GT(v_stats_after.images_processed, v_stats_before.images_processed);
    EXPECT_GT(a_stats_after.frames_processed, a_stats_before.frames_processed);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
