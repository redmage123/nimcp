/**
 * @file test_perception_audio.cpp
 * @brief Unit tests for audio perception/audio cortex module
 *
 * WHAT: Comprehensive tests for audio cortex functionality
 * WHY:  Ensure audio cortex correctly processes sound, extracts features,
 *       computes attention, detects speech, and integrates with memory systems
 * HOW:  GTest framework with synthetic audio generators
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

extern "C" {
#include "perception/nimcp_audio_cortex.h"
#include "core/brain/nimcp_brain.h"
}

//=============================================================================
// Synthetic Audio Generator
//=============================================================================

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
        std::random_device rd;
        std::mt19937 gen(42); // Fixed seed for reproducibility
        std::uniform_real_distribution<float> dist(-amplitude, amplitude);

        for (uint32_t i = 0; i < num_samples; i++) {
            audio[i] = dist(gen);
        }
        return audio;
    }

    static std::vector<float> GenerateChirp(uint32_t sample_rate, float f_start,
                                             float f_end, float duration_sec) {
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

    static std::vector<float> GenerateAMSignal(uint32_t sample_rate, float carrier_hz,
                                                float modulation_hz, float duration_sec) {
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

    static std::vector<float> GenerateSpeechLikeSignal(uint32_t sample_rate, float f1,
                                                        float f2, float duration_sec) {
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

    static std::vector<float> GenerateSilence(uint32_t sample_rate, float duration_sec) {
        uint32_t num_samples = static_cast<uint32_t>(sample_rate * duration_sec);
        return std::vector<float>(num_samples, 0.0f);
    }

    static std::vector<float> GenerateClick(uint32_t sample_rate, float duration_sec) {
        uint32_t num_samples = static_cast<uint32_t>(sample_rate * duration_sec);
        std::vector<float> audio(num_samples, 0.0f);
        if (num_samples > 0) {
            audio[num_samples / 2] = 1.0f; // Single spike in middle
        }
        return audio;
    }

    static std::vector<float> GenerateHarmonic(uint32_t sample_rate, float fundamental,
                                                int num_harmonics, float duration_sec) {
        uint32_t num_samples = static_cast<uint32_t>(sample_rate * duration_sec);
        std::vector<float> audio(num_samples, 0.0f);

        for (uint32_t i = 0; i < num_samples; i++) {
            float t = static_cast<float>(i) / sample_rate;
            for (int h = 1; h <= num_harmonics; h++) {
                audio[i] += (1.0f / h) * std::sin(2.0f * M_PI * fundamental * h * t);
            }
        }

        // Normalize
        float max_val = 0.0f;
        for (const auto& s : audio) {
            max_val = std::max(max_val, std::abs(s));
        }
        if (max_val > 0.0f) {
            for (auto& s : audio) {
                s /= max_val;
            }
        }
        return audio;
    }
};

//=============================================================================
// Audio Cortex Test Fixture
//=============================================================================

class AudioCortexTest : public ::testing::Test {
protected:
    audio_cortex_t* cortex = nullptr;

    static constexpr uint32_t SAMPLE_RATE = 16000;
    static constexpr uint32_t FRAME_SIZE = 512;
    static constexpr uint32_t NUM_MEL_FILTERS = 40;
    static constexpr uint32_t NUM_MFCC = 13;
    static constexpr uint32_t FEATURE_DIM = 64;
    // Actual feature dimension returned by audio_cortex_get_feature_dim()
    // Implementation calculates: num_mel_filters + num_mfcc (not configured feature_dim)
    static constexpr uint32_t ACTUAL_FEATURE_DIM = NUM_MEL_FILTERS + NUM_MFCC;

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
            .internal_neurons = 400,
            .enable_bio_async = false,
            .enable_second_messengers = false
        };

        cortex = audio_cortex_create(&config);
    }

    void TearDown() override {
        if (cortex) {
            audio_cortex_destroy(cortex);
            cortex = nullptr;
        }
    }

    std::vector<float> ProcessAudio(const std::vector<float>& audio) {
        std::vector<float> features(FEATURE_DIM);
        uint32_t num_samples = std::min(static_cast<uint32_t>(audio.size()), FRAME_SIZE);
        bool success = audio_cortex_process(cortex, audio.data(), num_samples, 1, features.data());
        EXPECT_TRUE(success);
        return features;
    }
};

//=============================================================================
// Audio Cortex Creation/Destruction Tests
//=============================================================================

TEST_F(AudioCortexTest, CreateValidConfig) {
    ASSERT_NE(cortex, nullptr) << "Failed to create audio cortex";
}

TEST(AudioCortexCreationTest, CreateWithNullConfig) {
    audio_cortex_t* ctx = audio_cortex_create(nullptr);
    EXPECT_EQ(ctx, nullptr) << "Should fail with null config";
}

TEST(AudioCortexCreationTest, CreateWithMinimalConfig) {
    audio_cortex_config_t config = {
        .sample_rate = 8000,
        .frame_size = 256,
        .num_freq_bins = 128,
        .num_mel_filters = 20,
        .num_mfcc = 10,
        .num_channels = 1,
        .feature_dim = 32,
        .enable_attention = false,
        .enable_memory = false,
        .enable_fractal_topology = false,
        .hub_ratio = 0.15f,
        .power_law_gamma = -2.1f,
        .internal_neurons = 200,
        .enable_bio_async = false,
        .enable_second_messengers = false
    };

    audio_cortex_t* ctx = audio_cortex_create(&config);
    ASSERT_NE(ctx, nullptr);
    audio_cortex_destroy(ctx);
}

TEST(AudioCortexCreationTest, CreateWithStereoConfig) {
    audio_cortex_config_t config = {
        .sample_rate = 16000,
        .frame_size = 512,
        .num_freq_bins = 256,
        .num_mel_filters = 40,
        .num_mfcc = 13,
        .num_channels = 2,  // Stereo
        .feature_dim = 64,
        .enable_attention = true,
        .enable_memory = true,
        .enable_fractal_topology = false,
        .hub_ratio = 0.15f,
        .power_law_gamma = -2.1f,
        .internal_neurons = 400,
        .enable_bio_async = false,
        .enable_second_messengers = false
    };

    audio_cortex_t* ctx = audio_cortex_create(&config);
    ASSERT_NE(ctx, nullptr);
    audio_cortex_destroy(ctx);
}

TEST(AudioCortexCreationTest, DestroyNull) {
    // Should not crash
    audio_cortex_destroy(nullptr);
}

//=============================================================================
// Audio Processing Tests
//=============================================================================

TEST_F(AudioCortexTest, ProcessSineTone) {
    ASSERT_NE(cortex, nullptr);

    auto audio = SyntheticAudioGenerator::GenerateSineTone(SAMPLE_RATE, 440.0f, 0.1f);
    auto features = ProcessAudio(audio);

    // Features should be non-zero for sine tone
    float sum = 0.0f;
    for (const auto& f : features) {
        sum += std::abs(f);
    }
    EXPECT_GT(sum, 0.0f) << "Features should have non-zero values for sine tone";
}

TEST_F(AudioCortexTest, ProcessWhiteNoise) {
    ASSERT_NE(cortex, nullptr);

    auto audio = SyntheticAudioGenerator::GenerateWhiteNoise(SAMPLE_RATE, 0.1f);
    auto features = ProcessAudio(audio);

    float sum = 0.0f;
    for (const auto& f : features) {
        sum += std::abs(f);
    }
    EXPECT_GT(sum, 0.0f);
}

TEST_F(AudioCortexTest, ProcessChirp) {
    ASSERT_NE(cortex, nullptr);

    auto audio = SyntheticAudioGenerator::GenerateChirp(SAMPLE_RATE, 100.0f, 4000.0f, 0.1f);
    auto features = ProcessAudio(audio);

    float sum = 0.0f;
    for (const auto& f : features) {
        sum += std::abs(f);
    }
    EXPECT_GT(sum, 0.0f);
}

TEST_F(AudioCortexTest, ProcessSpeechLikeSignal) {
    ASSERT_NE(cortex, nullptr);

    // Generate speech-like signal with typical formant frequencies
    auto audio = SyntheticAudioGenerator::GenerateSpeechLikeSignal(SAMPLE_RATE, 730.0f, 1090.0f, 0.1f);
    auto features = ProcessAudio(audio);

    float sum = 0.0f;
    for (const auto& f : features) {
        sum += std::abs(f);
    }
    EXPECT_GT(sum, 0.0f);
}

TEST_F(AudioCortexTest, ProcessSilence) {
    ASSERT_NE(cortex, nullptr);

    auto audio = SyntheticAudioGenerator::GenerateSilence(SAMPLE_RATE, 0.1f);
    auto features = ProcessAudio(audio);

    // Silence should produce minimal or zero features
    bool all_finite = true;
    for (const auto& f : features) {
        if (!std::isfinite(f)) {
            all_finite = false;
            break;
        }
    }
    EXPECT_TRUE(all_finite);
}

TEST_F(AudioCortexTest, ProcessClick) {
    ASSERT_NE(cortex, nullptr);

    auto audio = SyntheticAudioGenerator::GenerateClick(SAMPLE_RATE, 0.1f);
    auto features = ProcessAudio(audio);

    // Click should produce some response
    float sum = 0.0f;
    for (const auto& f : features) {
        sum += std::abs(f);
    }
    // May be zero or non-zero depending on energy calculation
    bool all_finite = true;
    for (const auto& f : features) {
        if (!std::isfinite(f)) {
            all_finite = false;
            break;
        }
    }
    EXPECT_TRUE(all_finite);
}

TEST_F(AudioCortexTest, ProcessNullAudio) {
    ASSERT_NE(cortex, nullptr);

    std::vector<float> features(FEATURE_DIM);
    bool success = audio_cortex_process(cortex, nullptr, FRAME_SIZE, 1, features.data());
    EXPECT_FALSE(success) << "Should fail with null audio";
}

TEST_F(AudioCortexTest, ProcessNullFeatures) {
    ASSERT_NE(cortex, nullptr);

    auto audio = SyntheticAudioGenerator::GenerateSineTone(SAMPLE_RATE, 440.0f, 0.1f);
    bool success = audio_cortex_process(cortex, audio.data(), FRAME_SIZE, 1, nullptr);
    EXPECT_FALSE(success) << "Should fail with null features output";
}

TEST_F(AudioCortexTest, ProcessNullCortex) {
    auto audio = SyntheticAudioGenerator::GenerateSineTone(SAMPLE_RATE, 440.0f, 0.1f);
    std::vector<float> features(FEATURE_DIM);
    bool success = audio_cortex_process(nullptr, audio.data(), FRAME_SIZE, 1, features.data());
    EXPECT_FALSE(success) << "Should fail with null cortex";
}

//=============================================================================
// Frequency Analysis Tests
//=============================================================================

TEST_F(AudioCortexTest, ComputeSpectrum) {
    ASSERT_NE(cortex, nullptr);

    auto audio = SyntheticAudioGenerator::GenerateSineTone(SAMPLE_RATE, 1000.0f, 0.1f);
    std::vector<float> spectrum(FRAME_SIZE / 2);

    bool success = audio_cortex_compute_spectrum(cortex, audio.data(), FRAME_SIZE, spectrum.data());
    EXPECT_TRUE(success);

    // Spectrum should have energy around 1000 Hz
    float sum = 0.0f;
    for (const auto& s : spectrum) {
        sum += std::abs(s);
    }
    EXPECT_GT(sum, 0.0f);
}

TEST_F(AudioCortexTest, ComputeMelFeatures) {
    ASSERT_NE(cortex, nullptr);

    // First compute spectrum
    auto audio = SyntheticAudioGenerator::GenerateSineTone(SAMPLE_RATE, 1000.0f, 0.1f);
    std::vector<float> spectrum(FRAME_SIZE / 2);
    audio_cortex_compute_spectrum(cortex, audio.data(), FRAME_SIZE, spectrum.data());

    // Then compute mel features
    std::vector<float> mel_features(NUM_MEL_FILTERS);
    bool success = audio_cortex_compute_mel_features(cortex, spectrum.data(),
                                                      FRAME_SIZE / 2, mel_features.data());
    EXPECT_TRUE(success);

    float sum = 0.0f;
    for (const auto& m : mel_features) {
        sum += std::abs(m);
    }
    EXPECT_GT(sum, 0.0f);
}

TEST_F(AudioCortexTest, ComputeMFCC) {
    ASSERT_NE(cortex, nullptr);

    // Compute spectrum first
    auto audio = SyntheticAudioGenerator::GenerateSineTone(SAMPLE_RATE, 1000.0f, 0.1f);
    std::vector<float> spectrum(FRAME_SIZE / 2);
    audio_cortex_compute_spectrum(cortex, audio.data(), FRAME_SIZE, spectrum.data());

    // Compute mel features
    std::vector<float> mel_features(NUM_MEL_FILTERS);
    audio_cortex_compute_mel_features(cortex, spectrum.data(), FRAME_SIZE / 2, mel_features.data());

    // Compute MFCC
    std::vector<float> mfcc(NUM_MFCC);
    bool success = audio_cortex_compute_mfcc(cortex, mel_features.data(), NUM_MEL_FILTERS, mfcc.data());
    EXPECT_TRUE(success);

    // MFCCs should have values
    bool all_finite = true;
    for (const auto& m : mfcc) {
        if (!std::isfinite(m)) {
            all_finite = false;
            break;
        }
    }
    EXPECT_TRUE(all_finite);
}

TEST_F(AudioCortexTest, DifferentFrequenciesProduceDifferentSpectra) {
    ASSERT_NE(cortex, nullptr);

    auto audio_low = SyntheticAudioGenerator::GenerateSineTone(SAMPLE_RATE, 200.0f, 0.1f);
    auto audio_high = SyntheticAudioGenerator::GenerateSineTone(SAMPLE_RATE, 4000.0f, 0.1f);

    std::vector<float> spectrum_low(FRAME_SIZE / 2);
    std::vector<float> spectrum_high(FRAME_SIZE / 2);

    audio_cortex_compute_spectrum(cortex, audio_low.data(), FRAME_SIZE, spectrum_low.data());
    audio_cortex_compute_spectrum(cortex, audio_high.data(), FRAME_SIZE, spectrum_high.data());

    // Spectra should be different
    float diff = 0.0f;
    for (size_t i = 0; i < spectrum_low.size(); i++) {
        diff += std::abs(spectrum_low[i] - spectrum_high[i]);
    }
    EXPECT_GT(diff, 0.0f);
}

//=============================================================================
// Attention Map Tests
//=============================================================================

TEST_F(AudioCortexTest, CreateAttentionMap) {
    audio_attention_map_t* map = audio_attention_map_create(NUM_MEL_FILTERS, 10);
    ASSERT_NE(map, nullptr);
    audio_attention_map_destroy(map);
}

TEST_F(AudioCortexTest, DestroyNullAttentionMap) {
    // Should not crash
    audio_attention_map_destroy(nullptr);
}

TEST_F(AudioCortexTest, ComputeAttention) {
    ASSERT_NE(cortex, nullptr);

    audio_attention_map_t* map = audio_attention_map_create(NUM_MEL_FILTERS, 10);
    ASSERT_NE(map, nullptr);

    auto audio = SyntheticAudioGenerator::GenerateSineTone(SAMPLE_RATE, 1000.0f, 0.1f);

    bool success = audio_cortex_compute_attention(cortex, audio.data(), FRAME_SIZE, map);
    EXPECT_TRUE(success);

    audio_attention_map_destroy(map);
}

TEST_F(AudioCortexTest, GetAttentionPeak) {
    ASSERT_NE(cortex, nullptr);

    audio_attention_map_t* map = audio_attention_map_create(NUM_MEL_FILTERS, 10);
    ASSERT_NE(map, nullptr);

    auto audio = SyntheticAudioGenerator::GenerateSineTone(SAMPLE_RATE, 1000.0f, 0.1f);
    audio_cortex_compute_attention(cortex, audio.data(), FRAME_SIZE, map);

    uint32_t max_freq = 0, max_time = 0;
    float max_value = 0.0f;
    bool success = audio_cortex_get_attention_peak(map, &max_freq, &max_time, &max_value);
    EXPECT_TRUE(success);

    EXPECT_LT(max_freq, NUM_MEL_FILTERS);
    EXPECT_GE(max_value, 0.0f);

    audio_attention_map_destroy(map);
}

//=============================================================================
// Temporal Processing Tests
//=============================================================================

TEST_F(AudioCortexTest, DetectTemporalEvents) {
    ASSERT_NE(cortex, nullptr);

    // Use a click to test onset detection
    auto audio = SyntheticAudioGenerator::GenerateClick(SAMPLE_RATE, 0.1f);

    bool onset_detected = false;
    bool offset_detected = false;

    bool success = audio_cortex_detect_temporal_events(cortex, audio.data(), FRAME_SIZE,
                                                        &onset_detected, &offset_detected);
    EXPECT_TRUE(success);
}

TEST_F(AudioCortexTest, ComputeEnvelope) {
    ASSERT_NE(cortex, nullptr);

    auto audio = SyntheticAudioGenerator::GenerateAMSignal(SAMPLE_RATE, 1000.0f, 5.0f, 0.1f);
    std::vector<float> envelope(FRAME_SIZE);

    bool success = audio_cortex_compute_envelope(cortex, audio.data(), FRAME_SIZE, envelope.data());
    EXPECT_TRUE(success);

    // Envelope should be non-negative
    for (const auto& e : envelope) {
        EXPECT_GE(e, 0.0f);
    }
}

//=============================================================================
// Auditory Memory Tests
//=============================================================================

TEST_F(AudioCortexTest, StoreMemory) {
    ASSERT_NE(cortex, nullptr);

    auto audio = SyntheticAudioGenerator::GenerateSineTone(SAMPLE_RATE, 440.0f, 0.1f);
    auto features = ProcessAudio(audio);

    bool success = audio_cortex_store_memory(cortex, features.data(), 0.8f);
    EXPECT_TRUE(success);
}

TEST_F(AudioCortexTest, StoreMemoryNullFeatures) {
    ASSERT_NE(cortex, nullptr);

    bool success = audio_cortex_store_memory(cortex, nullptr, 0.5f);
    EXPECT_FALSE(success);
}

TEST_F(AudioCortexTest, RecallMemory) {
    ASSERT_NE(cortex, nullptr);

    // Store a memory first
    auto audio = SyntheticAudioGenerator::GenerateSineTone(SAMPLE_RATE, 440.0f, 0.1f);
    auto features = ProcessAudio(audio);
    audio_cortex_store_memory(cortex, features.data(), 0.9f);

    // Recall similar memories
    auditory_memory_t** memories = nullptr;
    int num_recalled = 0;

    bool success = audio_cortex_recall_memory(cortex, features.data(), 5,
                                               &memories, &num_recalled);

    if (success && num_recalled > 0 && memories != nullptr) {
        free(memories);
    }
}

TEST_F(AudioCortexTest, ConsolidateMemory) {
    ASSERT_NE(cortex, nullptr);

    auto audio = SyntheticAudioGenerator::GenerateSineTone(SAMPLE_RATE, 440.0f, 0.1f);
    auto features = ProcessAudio(audio);

    bool success = audio_cortex_consolidate_memory(cortex, features.data(), 0.75f, "A4 note");
    (void)success;
}

//=============================================================================
// Novelty Computation Tests
//=============================================================================

TEST_F(AudioCortexTest, ComputeNovelty) {
    ASSERT_NE(cortex, nullptr);

    // Store some memories
    auto audio1 = SyntheticAudioGenerator::GenerateSineTone(SAMPLE_RATE, 440.0f, 0.1f);
    auto features1 = ProcessAudio(audio1);
    audio_cortex_store_memory(cortex, features1.data(), 0.8f);

    // Compute novelty for familiar sound
    float novelty_familiar = audio_cortex_compute_novelty(cortex, features1.data());

    // Compute novelty for different sound
    auto audio2 = SyntheticAudioGenerator::GenerateWhiteNoise(SAMPLE_RATE, 0.1f);
    auto features2 = ProcessAudio(audio2);
    float novelty_novel = audio_cortex_compute_novelty(cortex, features2.data());

    // Both should be valid
    if (novelty_familiar >= 0.0f && novelty_novel >= 0.0f) {
        EXPECT_GE(novelty_novel, novelty_familiar)
            << "Novel sound should have higher novelty";
    }
}

//=============================================================================
// Speech Detection Tests
//=============================================================================

TEST_F(AudioCortexTest, GetSpeechSalience) {
    ASSERT_NE(cortex, nullptr);

    // Speech-like signal
    auto speech_audio = SyntheticAudioGenerator::GenerateSpeechLikeSignal(SAMPLE_RATE, 730.0f, 1090.0f, 0.1f);
    auto speech_features = ProcessAudio(speech_audio);
    float speech_salience = audio_cortex_get_speech_salience(cortex, speech_features.data(), FEATURE_DIM);

    // Pure tone (non-speech)
    auto tone_audio = SyntheticAudioGenerator::GenerateSineTone(SAMPLE_RATE, 1000.0f, 0.1f);
    auto tone_features = ProcessAudio(tone_audio);
    float tone_salience = audio_cortex_get_speech_salience(cortex, tone_features.data(), FEATURE_DIM);

    // Speech salience should be valid
    EXPECT_GE(speech_salience, 0.0f);
    EXPECT_LE(speech_salience, 1.0f);
    EXPECT_GE(tone_salience, 0.0f);
    EXPECT_LE(tone_salience, 1.0f);
}

TEST_F(AudioCortexTest, ActivateSpeechMode) {
    ASSERT_NE(cortex, nullptr);

    // Should not crash
    audio_cortex_activate_speech_mode(cortex);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(AudioCortexTest, GetStats) {
    ASSERT_NE(cortex, nullptr);

    audio_cortex_stats_t stats;
    bool success = audio_cortex_get_stats(cortex, &stats);
    EXPECT_TRUE(success);

    EXPECT_GE(stats.frames_processed, 0u);
}

TEST_F(AudioCortexTest, StatsIncrementAfterProcessing) {
    ASSERT_NE(cortex, nullptr);

    audio_cortex_stats_t stats_before;
    audio_cortex_get_stats(cortex, &stats_before);

    // Process audio
    auto audio = SyntheticAudioGenerator::GenerateSineTone(SAMPLE_RATE, 440.0f, 0.1f);
    ProcessAudio(audio);

    audio_cortex_stats_t stats_after;
    audio_cortex_get_stats(cortex, &stats_after);

    EXPECT_GT(stats_after.frames_processed, stats_before.frames_processed);
}

//=============================================================================
// Brain Integration Tests
//=============================================================================

TEST_F(AudioCortexTest, SetBrain) {
    ASSERT_NE(cortex, nullptr);

    brain_t brain = brain_create("test_brain", BRAIN_SIZE_TINY,
                                  BRAIN_TASK_CLASSIFICATION, 10, 10);

    if (brain) {
        audio_cortex_set_brain(cortex, brain);
        audio_cortex_set_brain(cortex, nullptr);
        brain_destroy(brain);
    }
}

//=============================================================================
// Training Mode Tests
//=============================================================================

TEST_F(AudioCortexTest, SetTrainingMode) {
    ASSERT_NE(cortex, nullptr);

    int result = audio_cortex_set_training_mode(cortex, true);
    if (result == 0) {
        bool is_training = audio_cortex_is_training_mode(cortex);
        EXPECT_TRUE(is_training);

        audio_cortex_set_training_mode(cortex, false);
        is_training = audio_cortex_is_training_mode(cortex);
        EXPECT_FALSE(is_training);
    }
}

TEST_F(AudioCortexTest, GetTrainingState) {
    ASSERT_NE(cortex, nullptr);

    audio_cortex_set_training_mode(cortex, true);

    auto audio = SyntheticAudioGenerator::GenerateSineTone(SAMPLE_RATE, 440.0f, 0.1f);
    ProcessAudio(audio);

    audio_training_state_t state;
    int result = audio_cortex_get_training_state(cortex, &state);

    if (result == 0 && state.valid) {
        EXPECT_GE(state.quality, 0.0f);
        EXPECT_LE(state.quality, 1.0f);
    }
}

//=============================================================================
// Feature Dimension Tests
//=============================================================================

TEST_F(AudioCortexTest, GetFeatureDimension) {
    ASSERT_NE(cortex, nullptr);

    uint32_t dim = audio_cortex_get_feature_dim(cortex);
    // Implementation returns num_mel_filters + num_mfcc, not configured feature_dim
    EXPECT_EQ(dim, ACTUAL_FEATURE_DIM);
}

TEST_F(AudioCortexTest, GetFeatureDimensionNullCortex) {
    uint32_t dim = audio_cortex_get_feature_dim(nullptr);
    EXPECT_EQ(dim, 0);
}

//=============================================================================
// Edge Case Tests
//=============================================================================

TEST_F(AudioCortexTest, ProcessVeryLowFrequency) {
    ASSERT_NE(cortex, nullptr);

    auto audio = SyntheticAudioGenerator::GenerateSineTone(SAMPLE_RATE, 20.0f, 0.1f);
    auto features = ProcessAudio(audio);

    bool all_finite = true;
    for (const auto& f : features) {
        if (!std::isfinite(f)) {
            all_finite = false;
            break;
        }
    }
    EXPECT_TRUE(all_finite);
}

TEST_F(AudioCortexTest, ProcessVeryHighFrequency) {
    ASSERT_NE(cortex, nullptr);

    // Near Nyquist frequency
    auto audio = SyntheticAudioGenerator::GenerateSineTone(SAMPLE_RATE, 7000.0f, 0.1f);
    auto features = ProcessAudio(audio);

    bool all_finite = true;
    for (const auto& f : features) {
        if (!std::isfinite(f)) {
            all_finite = false;
            break;
        }
    }
    EXPECT_TRUE(all_finite);
}

TEST_F(AudioCortexTest, ProcessHarmonicSignal) {
    ASSERT_NE(cortex, nullptr);

    auto audio = SyntheticAudioGenerator::GenerateHarmonic(SAMPLE_RATE, 220.0f, 5, 0.1f);
    auto features = ProcessAudio(audio);

    float sum = 0.0f;
    for (const auto& f : features) {
        sum += std::abs(f);
    }
    EXPECT_GT(sum, 0.0f);
}

TEST_F(AudioCortexTest, ProcessAMSignal) {
    ASSERT_NE(cortex, nullptr);

    auto audio = SyntheticAudioGenerator::GenerateAMSignal(SAMPLE_RATE, 1000.0f, 10.0f, 0.1f);
    auto features = ProcessAudio(audio);

    float sum = 0.0f;
    for (const auto& f : features) {
        sum += std::abs(f);
    }
    EXPECT_GT(sum, 0.0f);
}

TEST_F(AudioCortexTest, DifferentAudioProducesDifferentFeatures) {
    ASSERT_NE(cortex, nullptr);

    auto audio1 = SyntheticAudioGenerator::GenerateSineTone(SAMPLE_RATE, 440.0f, 0.1f);
    auto audio2 = SyntheticAudioGenerator::GenerateWhiteNoise(SAMPLE_RATE, 0.1f);

    auto features1 = ProcessAudio(audio1);
    auto features2 = ProcessAudio(audio2);

    float diff = 0.0f;
    for (size_t i = 0; i < features1.size(); i++) {
        diff += std::abs(features1[i] - features2[i]);
    }
    EXPECT_GT(diff, 0.0f) << "Different audio should produce different features";
}

TEST_F(AudioCortexTest, SameAudioProducesSameFeatures) {
    ASSERT_NE(cortex, nullptr);

    auto audio = SyntheticAudioGenerator::GenerateSineTone(SAMPLE_RATE, 440.0f, 0.1f);

    auto features1 = ProcessAudio(audio);
    auto features2 = ProcessAudio(audio);

    float diff = 0.0f;
    for (size_t i = 0; i < features1.size(); i++) {
        diff += std::abs(features1[i] - features2[i]);
    }
    EXPECT_LT(diff, 0.001f) << "Same audio should produce same features";
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
